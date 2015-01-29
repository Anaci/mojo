// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/android/mojo_main.h"

#include "base/android/fifo_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "jni/MojoMain_jni.h"
#include "mojo/common/message_pump_mojo.h"
#include "shell/android/android_handler_loader.h"
#include "shell/android/background_application_loader.h"
#include "shell/android/native_viewport_application_loader.h"
#include "shell/android/ui_application_loader_android.h"
#include "shell/application_manager/application_loader.h"
#include "shell/command_line_util.h"
#include "shell/context.h"
#include "shell/init.h"
#include "ui/gl/gl_surface_egl.h"

using base::LazyInstance;

namespace mojo {
namespace shell {

namespace {

// Tag for logging.
const char kLogTag[] = "chromium";

// Command line argument for the communication fifo.
const char kFifoPath[] = "fifo-path";

class MojoShellRunner : public base::DelegateSimpleThread::Delegate {
 public:
  MojoShellRunner() {}
  ~MojoShellRunner() override {}

 private:
  void Run() override;

  DISALLOW_COPY_AND_ASSIGN(MojoShellRunner);
};

LazyInstance<scoped_ptr<base::MessageLoop>> g_java_message_loop =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<Context>> g_context = LAZY_INSTANCE_INITIALIZER;

LazyInstance<MojoShellRunner> g_shell_runner = LAZY_INSTANCE_INITIALIZER;

LazyInstance<scoped_ptr<base::DelegateSimpleThread>> g_shell_thread =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<base::android::ScopedJavaGlobalRef<jobject>> g_main_activiy =
    LAZY_INSTANCE_INITIALIZER;

void ConfigureAndroidServices(Context* context) {
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(new UIApplicationLoader(
          make_scoped_ptr(new NativeViewportApplicationLoader()),
          g_java_message_loop.Get().get())),
      GURL("mojo:native_viewport_service"));

  // Android handler is bundled with the Mojo shell, because it uses the
  // MojoShell application as the JNI bridge to bootstrap execution of other
  // Android Mojo apps that need JNI.
  context->application_manager()->SetLoaderForURL(
      make_scoped_ptr(new BackgroundApplicationLoader(
          make_scoped_ptr(new AndroidHandlerLoader()), "android_handler",
          base::MessageLoop::TYPE_DEFAULT)),
      GURL("mojo:android_handler"));

  // By default, the keyboard is handled by the native_viewport_service.
  context->mojo_url_resolver()->AddCustomMapping(
      GURL("mojo:keyboard"), GURL("mojo:native_viewport_service"));
}

void QuitShellThread() {
  g_shell_thread.Get()->Join();
  g_shell_thread.Pointer()->reset();
  Java_MojoMain_finishActivity(base::android::AttachCurrentThread(),
                               g_main_activiy.Get().obj());
  exit(0);
}

void MojoShellRunner::Run() {
  base::MessageLoop loop(common::MessagePumpMojo::Create());
  Context* context = g_context.Pointer()->get();
  ConfigureAndroidServices(context);
  context->Init();

  RunCommandLineApps(context);
  loop.Run();

  g_java_message_loop.Pointer()->get()->PostTask(FROM_HERE,
                                                 base::Bind(&QuitShellThread));
}

// Initialize stdout redirection if the command line switch is present.
void InitializeRedirection() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kFifoPath))
    return;

  base::FilePath fifo_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(kFifoPath);
  base::FilePath directory = fifo_path.DirName();
  CHECK(base::CreateDirectoryAndGetError(directory, nullptr))
      << "Unable to create directory: " << directory.value();
  unlink(fifo_path.value().c_str());
  CHECK(base::android::CreateFIFO(fifo_path, 0666))
      << "Unable to create fifo: " << fifo_path.value();
  CHECK(base::android::RedirectStream(stdout, fifo_path, "w"))
      << "Failed to redirect stdout to file: " << fifo_path.value();
  CHECK(dup2(STDOUT_FILENO, STDERR_FILENO) != -1)
      << "Unable to redirect stderr to stdout.";
}

}  // namespace

static void Init(JNIEnv* env,
                 jclass clazz,
                 jobject activity,
                 jstring mojo_shell_path,
                 jobjectArray jparameters,
                 jstring j_local_apps_directory) {
  g_main_activiy.Get().Reset(env, activity);

  base::android::ScopedJavaLocalRef<jobject> scoped_activity(env, activity);
  base::android::InitApplicationContext(env, scoped_activity);

  std::vector<std::string> parameters;
  parameters.push_back(
      base::android::ConvertJavaStringToUTF8(env, mojo_shell_path));
  base::android::AppendJavaStringArrayToStringVector(env, jparameters,
                                                     &parameters);

  // TODO(eseidel): Remove this as soon as we agree on a better solution!
  // Only here to unblock other work towards carry. http://crbug.com/451620
  if (parameters.size() == 1) {
    parameters.push_back("--origin=https://domokit.github.io/mojo");
    parameters.push_back(
        "--url-mappings=mojo:window_manager=mojo:kiosk_wm");
    parameters.push_back(
        "--args-for=mojo:window_manager https://domokit.github.io/home");
    parameters.push_back("mojo:window_manager");
  }

  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(parameters);

  InitializeLogging();

  InitializeRedirection();

  // We want ~MessageLoop to happen prior to ~Context. Initializing
  // LazyInstances is akin to stack-allocating objects; their destructors
  // will be invoked first-in-last-out.
  Context* shell_context = new Context();
  shell_context->SetShellFileRoot(base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, j_local_apps_directory)));
  for (auto& args : parameters)
    ApplyApplicationArgs(shell_context, args);

  g_context.Get().reset(shell_context);

  g_java_message_loop.Get().reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();

  // TODO(abarth): At which point should we switch to cross-platform
  // initialization?

  gfx::GLSurface::InitializeOneOff();
}

static jboolean Start(JNIEnv* env, jclass clazz) {
  if (!base::CommandLine::ForCurrentProcess()->GetArgs().size())
    return false;

#if defined(MOJO_SHELL_DEBUG_URL)
  base::CommandLine::ForCurrentProcess()->AppendArg(MOJO_SHELL_DEBUG_URL);
  // Sleep for 5 seconds to give the debugger a chance to attach.
  sleep(5);
#endif

  g_shell_thread.Get().reset(
      new base::DelegateSimpleThread(g_shell_runner.Pointer(), "ShellThread"));
  g_shell_thread.Get()->Start();
  return true;
}

static void AddApplicationURL(JNIEnv* env, jclass clazz, jstring jurl) {
  base::CommandLine::ForCurrentProcess()->AppendArg(
      base::android::ConvertJavaStringToUTF8(env, jurl));
}

bool RegisterMojoMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace shell
}  // namespace mojo
