// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "dart/runtime/include/dart_api.h"
#include "dart/runtime/include/dart_native_api.h"
#include "mojo/dart/embedder/builtin.h"
#include "mojo/dart/embedder/dart_controller.h"
#include "mojo/dart/embedder/mojo_dart_state.h"
#include "mojo/dart/embedder/observatory_archive.h"
#include "mojo/dart/embedder/vmservice.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/platform/dart/dart_handle_watcher.h"
#include "tonic/dart_converter.h"
#include "tonic/dart_debugger.h"
#include "tonic/dart_dependency_catcher.h"
#include "tonic/dart_error.h"
#include "tonic/dart_library_loader.h"
#include "tonic/dart_library_provider.h"
#include "tonic/dart_library_provider_files.h"
#include "tonic/dart_library_provider_network.h"
#include "tonic/dart_script_loader_sync.h"

namespace mojo {
namespace dart {

extern const uint8_t* vm_isolate_snapshot_buffer;
extern const uint8_t* isolate_snapshot_buffer;

static const char* kAsyncLibURL = "dart:async";
static const char* kInternalLibURL = "dart:_internal";
static const char* kIsolateLibURL = "dart:isolate";
static const char* kCoreLibURL = "dart:core";

static uint8_t snapshot_magic_number[] = { 0xf5, 0xf5, 0xdc, 0xdc };

static Dart_Handle SetWorkingDirectory(Dart_Handle builtin_lib) {
  base::FilePath current_dir;
  PathService::Get(base::DIR_CURRENT, &current_dir);

  const int kNumArgs = 1;
  Dart_Handle dart_args[kNumArgs];
  std::string current_dir_string = current_dir.AsUTF8Unsafe();
  dart_args[0] = Dart_NewStringFromUTF8(
      reinterpret_cast<const uint8_t*>(current_dir_string.data()),
      current_dir_string.length());
  return Dart_Invoke(builtin_lib,
                     Dart_NewStringFromCString("_setWorkingDirectory"),
                     kNumArgs,
                     dart_args);
}

static Dart_Handle ResolveScriptUri(Dart_Handle builtin_lib, Dart_Handle uri) {
  const int kNumArgs = 1;
  Dart_Handle dart_args[kNumArgs];
  dart_args[0] = uri;
  return Dart_Invoke(builtin_lib,
                     Dart_NewStringFromCString("_resolveScriptUri"),
                     kNumArgs,
                     dart_args);
}

static Dart_Handle PrepareBuiltinLibraries(const std::string& package_root,
                                           const std::string& script_uri) {
  // First ensure all required libraries are available.
  Dart_Handle builtin_lib = Builtin::PrepareLibrary(Builtin::kBuiltinLibrary);
  Builtin::PrepareLibrary(Builtin::kMojoInternalLibrary);
  Builtin::PrepareLibrary(Builtin::kDartMojoIoLibrary);
  Dart_Handle url = Dart_NewStringFromCString(kInternalLibURL);
  DART_CHECK_VALID(url);
  Dart_Handle internal_lib = Dart_LookupLibrary(url);
  DART_CHECK_VALID(internal_lib);
  url = Dart_NewStringFromCString(kCoreLibURL);
  DART_CHECK_VALID(url);
  Dart_Handle core_lib = Dart_LookupLibrary(url);
  DART_CHECK_VALID(internal_lib);
  url = Dart_NewStringFromCString(kAsyncLibURL);
  DART_CHECK_VALID(url);
  Dart_Handle async_lib = Dart_LookupLibrary(url);
  DART_CHECK_VALID(async_lib);
  url = Dart_NewStringFromCString(kIsolateLibURL);
  DART_CHECK_VALID(url);
  Dart_Handle isolate_lib = Dart_LookupLibrary(url);
  DART_CHECK_VALID(isolate_lib);
  Dart_Handle mojo_internal_lib =
      Builtin::GetLibrary(Builtin::kMojoInternalLibrary);
  DART_CHECK_VALID(mojo_internal_lib);

  // We need to ensure that all the scripts loaded so far are finalized
  // as we are about to invoke some Dart code below to setup closures.
  Dart_Handle result = Dart_FinalizeLoading(false);
  DART_CHECK_VALID(result);

  // Import dart:_internal into dart:mojo.builtin for setting up hooks.
  result = Dart_LibraryImportLibrary(builtin_lib, internal_lib, Dart_Null());
  DART_CHECK_VALID(result);

  // Setup the internal library's 'internalPrint' function.
  Dart_Handle print = Dart_Invoke(builtin_lib,
                                  Dart_NewStringFromCString("_getPrintClosure"),
                                  0,
                                  nullptr);
  DART_CHECK_VALID(print);
  result = Dart_SetField(internal_lib,
                         Dart_NewStringFromCString("_printClosure"),
                         print);
  DART_CHECK_VALID(result);

  DART_CHECK_VALID(Dart_Invoke(
      builtin_lib, Dart_NewStringFromCString("_setupHooks"), 0, nullptr));
  DART_CHECK_VALID(Dart_Invoke(
      isolate_lib, Dart_NewStringFromCString("_setupHooks"), 0, nullptr));

  // Setup the 'scheduleImmediate' closure.
  Dart_Handle schedule_immediate_closure = Dart_Invoke(
      isolate_lib,
      Dart_NewStringFromCString("_getIsolateScheduleImmediateClosure"),
      0,
      nullptr);
  Dart_Handle schedule_args[1];
  schedule_args[0] = schedule_immediate_closure;
  result = Dart_Invoke(
      async_lib,
      Dart_NewStringFromCString("_setScheduleImmediateClosure"),
      1,
      schedule_args);
  DART_CHECK_VALID(result);

  // Set current working directory.
  result = SetWorkingDirectory(builtin_lib);
  if (Dart_IsError(result)) {
    return result;
  }

  // Set script entry uri.
  Dart_Handle uri = Dart_NewStringFromUTF8(
      reinterpret_cast<const uint8_t*>(script_uri.c_str()),
      script_uri.length());
  DART_CHECK_VALID(uri);
  result = ResolveScriptUri(builtin_lib, uri);

  // Set up package root.
  result = Dart_NewStringFromUTF8(
      reinterpret_cast<const uint8_t*>(package_root.c_str()),
      package_root.length());
  DART_CHECK_VALID(result);

  const int kNumArgs = 1;
  Dart_Handle dart_args[kNumArgs];
  dart_args[0] = result;
  result = Dart_Invoke(builtin_lib,
                     Dart_NewStringFromCString("_setPackageRoot"),
                     kNumArgs,
                     dart_args);
  DART_CHECK_VALID(result);

  // Setup the uriBase with the base uri of the mojo app.
  Dart_Handle uri_base = Dart_Invoke(
      builtin_lib,
      Dart_NewStringFromCString("_getUriBaseClosure"),
      0,
      nullptr);
  DART_CHECK_VALID(uri_base);
  result = Dart_SetField(core_lib,
                         Dart_NewStringFromCString("_uriBaseClosure"),
                         uri_base);
  DART_CHECK_VALID(result);
  return result;
}

static const intptr_t kStartIsolateArgumentsLength = 2;

static void SetupStartIsolateArguments(
    const DartControllerConfig& config,
    Dart_Handle main_closure,
    Dart_Handle* start_isolate_args) {
  // start_isolate_args:
  // [0] -> main closure
  // [1] -> args list.
  // args list:
  // [0] -> mojo handle.
  // [1] -> script uri
  // [2] -> list of script arguments in config.
  start_isolate_args[0] = main_closure;     // entryPoint
  DART_CHECK_VALID(start_isolate_args[0]);
  start_isolate_args[1] = Dart_NewList(3);  // args
  DART_CHECK_VALID(start_isolate_args[1]);
  Dart_Handle script_uri = Dart_NewStringFromUTF8(
      reinterpret_cast<const uint8_t*>(config.script_uri.data()),
      config.script_uri.length());
  Dart_ListSetAt(start_isolate_args[1], 0, Dart_NewInteger(config.handle));
  Dart_ListSetAt(start_isolate_args[1], 1, script_uri);
  Dart_Handle script_args = Dart_NewList(config.script_flags_count);
  DART_CHECK_VALID(script_args);
  Dart_ListSetAt(start_isolate_args[1], 2, script_args);
  for (intptr_t i = 0; i < config.script_flags_count; i++) {
    Dart_ListSetAt(script_args, i,
                   Dart_NewStringFromCString(config.script_flags[i]));
  }
}

static void CloseHandlesOnError(Dart_Handle error) {
  if (!Dart_IsError(error)) {
    return;
  }

  Dart_Handle mojo_core_lib =
      Builtin::GetLibrary(Builtin::kMojoInternalLibrary);
  DART_CHECK_VALID(mojo_core_lib);
  Dart_Handle handle_natives_type =
      Dart_GetType(mojo_core_lib,
                   Dart_NewStringFromCString("MojoHandleNatives"), 0, nullptr);
  DART_CHECK_VALID(handle_natives_type);
  Dart_Handle method_name = Dart_NewStringFromCString("_closeOpenHandles");
  CHECK(!Dart_IsError(method_name));
  Dart_Handle result =
      Dart_Invoke(handle_natives_type, method_name, 0, nullptr);
  DART_CHECK_VALID(result);

  auto isolate_data = MojoDartState::Current();
  if (!isolate_data->callbacks().exception.is_null()) {
    int64_t handles_closed = 0;
    Dart_Handle int_result = Dart_IntegerToInt64(result, &handles_closed);
    DART_CHECK_VALID(int_result);
    isolate_data->callbacks().exception.Run(error, handles_closed);
  }
}

static void RunIsolate(Dart_Isolate isolate,
                       const DartControllerConfig& config) {
  tonic::DartIsolateScope isolate_scope(isolate);
  tonic::DartApiScope api_scope;

  Dart_Handle result;

  // Load the root library into the builtin library so that main can be found.
  Dart_Handle builtin_lib =
      Builtin::GetLibrary(Builtin::kBuiltinLibrary);
  DART_CHECK_VALID(builtin_lib);
  Dart_Handle root_lib = Dart_RootLibrary();
  DART_CHECK_VALID(root_lib);
  result = Dart_LibraryImportLibrary(builtin_lib, root_lib, Dart_Null());
  DART_CHECK_VALID(result);

  if (config.compile_all) {
    result = Dart_CompileAll();
    DART_CHECK_VALID(result);
  }

  Dart_Handle main_closure = Dart_Invoke(
      builtin_lib,
      Dart_NewStringFromCString("_getMainClosure"),
      0,
      nullptr);
  DART_CHECK_VALID(main_closure);

  Dart_Handle start_isolate_args[kStartIsolateArgumentsLength];
  SetupStartIsolateArguments(config, main_closure, &start_isolate_args[0]);
  Dart_Handle isolate_lib =
      Dart_LookupLibrary(Dart_NewStringFromCString(kIsolateLibURL));
  DART_CHECK_VALID(isolate_lib);

  result = Dart_Invoke(isolate_lib,
                       Dart_NewStringFromCString("_startMainIsolate"),
                       kStartIsolateArgumentsLength,
                       start_isolate_args);
  DART_CHECK_VALID(result);

  result = Dart_RunLoop();
  CloseHandlesOnError(result);

  // Here we log the error, but we don't do DART_CHECK_VALID because we don't
  // want to bring the whole process down due to an error in application code,
  // whereas above we do want to bring the whole process down for a bug in
  // library or generated code.
  tonic::LogIfError(result);
}

Dart_Handle DartController::LibraryTagHandler(Dart_LibraryTag tag,
                                              Dart_Handle library,
                                              Dart_Handle url) {
  if (tag == Dart_kCanonicalizeUrl) {
    std::string string = tonic::StdStringFromDart(url);
    if (StartsWithASCII(string, "dart:", true))
      return url;
  }
  return tonic::DartLibraryLoader::HandleLibraryTag(tag, library, url);
}

Dart_Isolate DartController::CreateIsolateHelper(
    void* dart_app,
    bool strict_compilation,
    IsolateCallbacks callbacks,
    std::string script_uri,
    const std::string& package_root,
    char** error,
    bool use_network_loader) {
  auto isolate_data = new MojoDartState(dart_app,
                                        strict_compilation,
                                        callbacks,
                                        script_uri,
                                        package_root);
  CHECK(isolate_snapshot_buffer != nullptr);
  Dart_Isolate isolate =
      Dart_CreateIsolate(script_uri.c_str(), "main", isolate_snapshot_buffer,
                         nullptr, isolate_data, error);
  if (isolate == nullptr) {
    delete isolate_data;
    return nullptr;
  }
  Dart_ExitIsolate();

  isolate_data->SetIsolate(isolate);
  if (service_connector_ != nullptr) {
    // This is not supported in the unit test harness.
    isolate_data->BindNetworkService(
        service_connector_->ConnectToService(
            DartControllerServiceConnector::kNetworkServiceId));
  }

  // Setup isolate and load script.
  {
    tonic::DartIsolateScope isolate_scope(isolate);
    tonic::DartApiScope api_scope;
    // Setup loader.
    const char* package_root_str = nullptr;
    if (package_root.empty()) {
      package_root_str = "/";
    } else {
      package_root_str = package_root.c_str();
    }
    isolate_data->library_loader().set_magic_number(
        snapshot_magic_number, sizeof(snapshot_magic_number));
    if (use_network_loader) {
      mojo::NetworkService* network_service = isolate_data->network_service();
      isolate_data->set_library_provider(
          new tonic::DartLibraryProviderNetwork(network_service));
    } else {
      isolate_data->set_library_provider(
          new tonic::DartLibraryProviderFiles(
              base::FilePath(package_root_str)));
    }
    Dart_Handle result = Dart_SetLibraryTagHandler(LibraryTagHandler);
    DART_CHECK_VALID(result);
    // Toggle checked mode.
    Dart_IsolateSetStrictCompilation(strict_compilation);
    // Prepare builtin and its dependent libraries.
    result = PrepareBuiltinLibraries(package_root, script_uri);
    DART_CHECK_VALID(result);

    // Set the handle watcher's control handle in the spawning isolate.
    result = SetHandleWatcherControlHandle();
    DART_CHECK_VALID(result);

    // The VM is creating the service isolate.
    if (Dart_IsServiceIsolate(isolate)) {
      service_isolate_spawned_ = true;
      const intptr_t port =
          (SupportDartMojoIo() && observatory_enabled_) ? 0 : -1;
      InitializeDartMojoIo();
      if (!VmService::Setup("127.0.0.1", port)) {
        *error = strdup(VmService::GetErrorMessage());
        return nullptr;
      }
      return isolate;
    }

    tonic::DartScriptLoaderSync::LoadScript(
        script_uri,
        isolate_data->library_provider());
    InitializeDartMojoIo();
  }

  if (isolate_data->library_loader().error_during_loading()) {
    *error = strdup("Library loader reported error during loading. See log.");
    Dart_EnterIsolate(isolate);
    Dart_ShutdownIsolate();
    return nullptr;
  }

  // Make the isolate runnable so that it is ready to handle messages.
  bool retval = Dart_IsolateMakeRunnable(isolate);
  if (!retval) {
    *error = strdup("Invalid isolate state - Unable to make it runnable");
    Dart_EnterIsolate(isolate);
    Dart_ShutdownIsolate();
    return nullptr;
  }

  DCHECK(Dart_CurrentIsolate() == nullptr);

  return isolate;
}

Dart_Handle DartController::SetHandleWatcherControlHandle() {
  CHECK(handle_watcher_producer_handle_ != MOJO_HANDLE_INVALID);
  Dart_Handle mojo_internal_lib =
      Builtin::GetLibrary(Builtin::kMojoInternalLibrary);
  DART_CHECK_VALID(mojo_internal_lib);
  Dart_Handle handle_watcher_type = Dart_GetType(
      mojo_internal_lib,
      Dart_NewStringFromCString("MojoHandleWatcher"),
      0,
      nullptr);
  DART_CHECK_VALID(handle_watcher_type);
  Dart_Handle field_name = Dart_NewStringFromCString("mojoControlHandle");
  DART_CHECK_VALID(field_name);
  Dart_Handle control_port_value =
      Dart_NewInteger(handle_watcher_producer_handle_);
  Dart_Handle result =
      Dart_SetField(handle_watcher_type, field_name, control_port_value);
  return result;
}

Dart_Isolate DartController::IsolateCreateCallback(const char* script_uri,
                                                   const char* main,
                                                   const char* package_root,
                                                   const char** package_map,
                                                   Dart_IsolateFlags* flags,
                                                   void* callback_data,
                                                   char** error) {
  auto parent_isolate_data = MojoDartState::Cast(callback_data);
  std::string script_uri_string;
  std::string package_root_string;

  if (script_uri == nullptr) {
    if (callback_data == nullptr) {
      *error = strdup("Invalid 'callback_data' - Unable to spawn new isolate");
      return nullptr;
    }
    script_uri_string = parent_isolate_data->script_uri();
  } else {
    script_uri_string = std::string(script_uri);
  }
  if (package_root == nullptr) {
    if (parent_isolate_data != nullptr) {
      package_root_string = parent_isolate_data->package_root();
    }
  } else {
    package_root_string = std::string(package_root);
  }
  // Inherit parameters from parent isolate (if any).
  void* dart_app = nullptr;
  bool strict_compilation = false;
  // TODO(johnmccutchan): Use parent's setting?
  bool use_network_loader = false;
  IsolateCallbacks callbacks;
  if (parent_isolate_data != nullptr) {
    dart_app = parent_isolate_data->application_data();
    strict_compilation = parent_isolate_data->strict_compilation();
    callbacks = parent_isolate_data->callbacks();
  }
  return CreateIsolateHelper(dart_app,
                             strict_compilation,
                             callbacks,
                             script_uri_string,
                             package_root_string,
                             error,
                             use_network_loader);
}

void DartController::IsolateShutdownCallback(void* callback_data) {
  {
    tonic::DartApiScope api_scope;

    // Shut down dart:io.
    ShutdownDartMojoIo();
  }

  auto isolate_data = MojoDartState::Cast(callback_data);
  delete isolate_data;
}


bool DartController::initialized_ = false;
MojoHandle DartController::handle_watcher_producer_handle_ =
    MOJO_HANDLE_INVALID;
bool DartController::service_isolate_running_ = false;
bool DartController::service_isolate_spawned_ = false;
bool DartController::strict_compilation_ = false;
bool DartController::observatory_enabled_ = true;
DartControllerServiceConnector* DartController::service_connector_ = nullptr;
base::Lock DartController::lock_;

bool DartController::SupportDartMojoIo() {
  return service_connector_ != nullptr;
}

void DartController::InitializeDartMojoIo() {
  Dart_Isolate current_isolate = Dart_CurrentIsolate();
  CHECK(current_isolate != nullptr);
  if (!SupportDartMojoIo()) {
    return;
  }
  CHECK(service_connector_ != nullptr);
  // Get handles to the network and files services.
  MojoHandle network_service_mojo_handle = MOJO_HANDLE_INVALID;
  network_service_mojo_handle =
        service_connector_->ConnectToService(
            DartControllerServiceConnector::kNetworkServiceId);
  MojoHandle files_service_mojo_handle = MOJO_HANDLE_INVALID;
  files_service_mojo_handle =
      service_connector_->ConnectToService(
          DartControllerServiceConnector::kFilesServiceId);
  if ((network_service_mojo_handle == MOJO_HANDLE_INVALID) &&
      (files_service_mojo_handle == MOJO_HANDLE_INVALID)) {
    // Not supported.
    return;
  }
  // Pass handles into 'dart:io' library.
  Dart_Handle mojo_io_library =
      Builtin::GetLibrary(Builtin::kDartMojoIoLibrary);
  CHECK(!Dart_IsError(mojo_io_library));
  Dart_Handle method_name = Dart_NewStringFromCString("_initialize");
  CHECK(!Dart_IsError(method_name));
  Dart_Handle network_service_handle =
      Dart_NewInteger(network_service_mojo_handle);
  CHECK(!Dart_IsError(network_service_handle));
  Dart_Handle files_service_handle =
      Dart_NewInteger(files_service_mojo_handle);
  CHECK(!Dart_IsError(files_service_handle));
  Dart_Handle arguments[] = {
    network_service_handle,
    files_service_handle,
  };
  Dart_Handle result = Dart_Invoke(mojo_io_library,
                                   method_name,
                                   2,
                                   &arguments[0]);
  CHECK(!Dart_IsError(result));
}

void DartController::ShutdownDartMojoIo() {
  Dart_Isolate current_isolate = Dart_CurrentIsolate();
  CHECK(current_isolate != nullptr);
  if (!SupportDartMojoIo()) {
    return;
  }
  Dart_Handle mojo_io_library =
      Builtin::GetLibrary(Builtin::kDartMojoIoLibrary);
  CHECK(!Dart_IsError(mojo_io_library));
  Dart_Handle method_name = Dart_NewStringFromCString("_shutdown");
  CHECK(!Dart_IsError(method_name));
  Dart_Handle result = Dart_Invoke(mojo_io_library,
                                   method_name,
                                   0,
                                   nullptr);
  if (Dart_IsError(result)) {
    Dart_PropagateError(result);
  }
}


static Dart_Handle MakeUint8Array(const uint8_t* buffer,
                                  unsigned int buffer_len) {
  const intptr_t len = static_cast<intptr_t>(buffer_len);
  Dart_Handle array = Dart_NewTypedData(Dart_TypedData_kUint8, len);
  DART_CHECK_VALID(array);
  {
    Dart_TypedData_Type td_type;
    void* td_data = nullptr;
    intptr_t td_len = 0;
    Dart_Handle result =
        Dart_TypedDataAcquireData(array, &td_type, &td_data, &td_len);
    DART_CHECK_VALID(result);
    CHECK_EQ(td_type, Dart_TypedData_kUint8);
    CHECK(td_data != nullptr);
    CHECK_EQ(td_len, len);
    memmove(td_data, buffer, td_len);
    result = Dart_TypedDataReleaseData(array);
    DART_CHECK_VALID(result);
  }
  return array;
}

static Dart_Handle GetVMServiceAssetsArchiveCallback() {
  return MakeUint8Array(
      ::dart::observatory::observatory_assets_archive,
      ::dart::observatory::observatory_assets_archive_len);
}

void DartController::InitVmIfNeeded(Dart_EntropySource entropy,
                                    const char** vm_flags,
                                    int vm_flags_count) {
  base::AutoLock al(lock_);
  if (initialized_) {
    return;
  }

  // Start a handle watcher.
  handle_watcher_producer_handle_ = HandleWatcher::Start();

  const char* kControllerFlags[] = {
    // TODO(zra): Fix Dart VM Shutdown race.
    // There is a bug in Dart VM shutdown which causes its thread pool threads
    // to potentially fail to exit when the rest of the VM is going down. This
    // results in a segfault if they begin running again after the Dart
    // embedder has been unloaded. Setting this flag to 0 ensures that these
    // threads sleep forever instead of waking up and trying to run code
    // that isn't there anymore.
    "--worker-timeout-millis=0",
    // Disable access dart:mirrors library.
    "--enable_mirrors=false",
  };

  // Number of flags the controller sets.
  const int kNumControllerFlags = arraysize(kControllerFlags);
  const int kNumFlags = vm_flags_count + kNumControllerFlags;
  const char* flags[kNumFlags];

  for (int i = 0; i < kNumControllerFlags; i++) {
    flags[i] = kControllerFlags[i];
  }

  for (int i = 0; i < vm_flags_count; ++i) {
    flags[i + kNumControllerFlags] = vm_flags[i];
  }

  bool result = Dart_SetVMFlags(kNumFlags, flags);
  CHECK(result);

  // This should be called before calling Dart_Initialize.
  tonic::DartDebugger::InitDebugger();

  const char* error = Dart_Initialize(
      vm_isolate_snapshot_buffer,
      nullptr, // Precompiled instructions
      IsolateCreateCallback,
      nullptr,  // Deprecated isolate interrupt callback. Must pass nullptr.
      nullptr,  // Deprecated unhandled exception callback. Must pass nullptr.
      IsolateShutdownCallback,
      // File IO callbacks.
      nullptr, nullptr, nullptr, nullptr,
      entropy,
      GetVMServiceAssetsArchiveCallback);
  CHECK(error == nullptr);
  initialized_ = true;
}

void DartController::BlockForServiceIsolate() {
  base::AutoLock al(lock_);
  BlockForServiceIsolateLocked();
}

void DartController::BlockForServiceIsolateLocked() {
  if (service_isolate_running_) {
    return;
  }
  // By waiting for the load port, we ensure that the service isolate is fully
  // running before returning.
  Dart_ServiceWaitForLoadPort();
  service_isolate_running_ = true;
}

static bool GenerateEntropy(uint8_t* buffer, intptr_t length) {
  base::RandBytes(reinterpret_cast<void*>(buffer), length);
  return true;
}

bool DartController::Initialize(
    DartControllerServiceConnector* service_connector,
    bool strict_compilation,
    bool observatory_enabled,
    const char** extra_args,
    int extra_args_count) {
  service_connector_ = service_connector;
  observatory_enabled_ = observatory_enabled;
  strict_compilation_ = strict_compilation;
  InitVmIfNeeded(GenerateEntropy, extra_args, extra_args_count);
  return true;
}

bool DartController::RunDartScript(const DartControllerConfig& config) {
  const bool strict = strict_compilation_ || config.strict_compilation;
  Dart_Isolate isolate = CreateIsolateHelper(config.application_data,
                                             strict,
                                             config.callbacks,
                                             config.script_uri,
                                             config.package_root,
                                             config.error,
                                             config.use_network_loader);
  if (isolate == nullptr) {
    return false;
  }

  RunIsolate(isolate, config);

  // Cleanup.
  Dart_EnterIsolate(isolate);
  Dart_ShutdownIsolate();

  return true;
}

void DartController::Shutdown() {
  base::AutoLock al(lock_);
  if (!initialized_) {
    return;
  }
  BlockForServiceIsolateLocked();
  Dart_Cleanup();
  service_isolate_running_ = false;
  initialized_ = false;
}

}  // namespace apps
}  // namespace mojo
