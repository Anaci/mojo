// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/gles2/command_buffer_driver.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "services/gles2/command_buffer_type_conversions.h"
#include "services/gles2/mojo_buffer_backing.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace mojo {

namespace {

class MemoryTrackerStub : public gpu::gles2::MemoryTracker {
 public:
  MemoryTrackerStub() {}

  void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool pool) override {}

  bool EnsureGPUMemoryAvailable(size_t size_needed) override { return true; };

 private:
  ~MemoryTrackerStub() override {}

  DISALLOW_COPY_AND_ASSIGN(MemoryTrackerStub);
};

}  // anonymous namespace

CommandBufferDriver::Client::~Client() {
}

CommandBufferDriver::CommandBufferDriver(
    gfx::GLShareGroup* share_group,
    gpu::gles2::MailboxManager* mailbox_manager,
    gpu::SyncPointManager* sync_point_manager)
    : CommandBufferDriver(gfx::kNullAcceleratedWidget,
                          gfx::Size(1, 1),
                          share_group,
                          mailbox_manager,
                          sync_point_manager) {
}

CommandBufferDriver::CommandBufferDriver(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::GLShareGroup* share_group,
    gpu::gles2::MailboxManager* mailbox_manager,
    gpu::SyncPointManager* sync_point_manager)
    : client_(nullptr),
      widget_(widget),
      size_(size),
      share_group_(share_group),
      mailbox_manager_(mailbox_manager),
      sync_point_manager_(sync_point_manager),
      weak_factory_(this) {
}

CommandBufferDriver::~CommandBufferDriver() {
  if (decoder_) {
    bool have_context = decoder_->MakeCurrent();
    decoder_->Destroy(have_context);
  }
  client_->DidDestroy();
}

void CommandBufferDriver::Initialize(CommandBufferSyncClientPtr sync_client,
                                     ScopedSharedBufferHandle shared_state) {
  sync_client_ = sync_client.Pass();
  bool success = DoInitialize(shared_state.Pass());
  GpuCapabilitiesPtr capabilities =
      success ? GpuCapabilities::From(decoder_->GetCapabilities())
              : GpuCapabilities::New();
  sync_client_->DidInitialize(success, capabilities.Pass());
}

bool CommandBufferDriver::DoInitialize(ScopedSharedBufferHandle shared_state) {
  if (widget_ == gfx::kNullAcceleratedWidget)
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(size_);
  else {
    surface_ = gfx::GLSurface::CreateViewGLSurface(widget_);
    if (auto vsync_provider = surface_->GetVSyncProvider()) {
      vsync_provider->GetVSyncParameters(
          base::Bind(&CommandBufferDriver::OnUpdateVSyncParameters,
                     weak_factory_.GetWeakPtr()));
    }
  }

  if (!surface_.get())
    return false;

  // TODO(piman): virtual contexts, gpu preference.
  context_ = gfx::GLContext::CreateGLContext(share_group_.get(), surface_.get(),
                                             gfx::PreferIntegratedGpu);
  if (!context_.get())
    return false;

  if (!context_->MakeCurrent(surface_.get()))
    return false;

  // TODO(piman): ShaderTranslatorCache is currently per-ContextGroup but
  // only needs to be per-thread.
  scoped_refptr<gpu::gles2::ContextGroup> context_group =
      new gpu::gles2::ContextGroup(
          mailbox_manager_.get(), new MemoryTrackerStub,
          new gpu::gles2::ShaderTranslatorCache, nullptr, nullptr, true);

  command_buffer_.reset(
      new gpu::CommandBufferService(context_group->transfer_buffer_manager()));
  bool result = command_buffer_->Initialize();
  DCHECK(result);

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group.get()));
  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(), decoder_.get(),
                                         decoder_.get()));
  decoder_->set_engine(scheduler_.get());
  decoder_->SetResizeCallback(
      base::Bind(&CommandBufferDriver::OnResize, base::Unretained(this)));
  decoder_->SetWaitSyncPointCallback(base::Bind(
      &CommandBufferDriver::OnWaitSyncPoint, base::Unretained(this)));

  gpu::gles2::DisallowedFeatures disallowed_features;

  // TODO(piman): attributes.
  std::vector<int32> attrib_vector;
  if (!decoder_->Initialize(surface_, context_, false /* offscreen */, size_,
                            disallowed_features, attrib_vector))
    return false;

  command_buffer_->SetPutOffsetChangeCallback(base::Bind(
      &gpu::GpuScheduler::PutChanged, base::Unretained(scheduler_.get())));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &gpu::GpuScheduler::SetGetBuffer, base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&CommandBufferDriver::OnParseError, base::Unretained(this)));

  // TODO(piman): other callbacks

  const size_t kSize = sizeof(gpu::CommandBufferSharedState);
  scoped_ptr<gpu::BufferBacking> backing(
      gles2::MojoBufferBacking::Create(shared_state.Pass(), kSize));
  if (!backing)
    return false;

  command_buffer_->SetSharedStateBuffer(backing.Pass());
  return true;
}

void CommandBufferDriver::SetGetBuffer(int32_t buffer) {
  command_buffer_->SetGetBuffer(buffer);
}

void CommandBufferDriver::Flush(int32_t put_offset) {
  if (!context_->MakeCurrent(surface_.get())) {
    DLOG(WARNING) << "Context lost";
    OnContextLost(gpu::error::kUnknown);
    return;
  }
  command_buffer_->Flush(put_offset);
}

void CommandBufferDriver::MakeProgress(int32_t last_get_offset) {
  // TODO(piman): handle out-of-order.
  sync_client_->DidMakeProgress(
      CommandBufferState::From(command_buffer_->GetLastState()));
}

void CommandBufferDriver::RegisterTransferBuffer(
    int32_t id,
    ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  // Take ownership of the memory and map it into this process.
  // This validates the size.
  scoped_ptr<gpu::BufferBacking> backing(
      gles2::MojoBufferBacking::Create(transfer_buffer.Pass(), size));
  if (!backing) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }
  command_buffer_->RegisterTransferBuffer(id, backing.Pass());
}

void CommandBufferDriver::DestroyTransferBuffer(int32_t id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferDriver::Echo(const Callback<void()>& callback) {
  callback.Run();
}

void CommandBufferDriver::OnParseError() {
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  OnContextLost(state.context_lost_reason);
}

void CommandBufferDriver::OnResize(gfx::Size size, float scale_factor) {
  surface_->Resize(size);
}

bool CommandBufferDriver::OnWaitSyncPoint(uint32_t sync_point) {
  if (!sync_point)
    return true;
  if (sync_point_manager_->IsSyncPointRetired(sync_point))
    return true;
  scheduler_->SetScheduled(false);
  sync_point_manager_->AddSyncPointCallback(
      sync_point, base::Bind(&CommandBufferDriver::OnSyncPointRetired,
                             weak_factory_.GetWeakPtr()));
  return scheduler_->IsScheduled();
}

void CommandBufferDriver::OnSyncPointRetired() {
  scheduler_->SetScheduled(true);
}

void CommandBufferDriver::OnContextLost(uint32_t reason) {
  client_->LostContext(reason);
}

void CommandBufferDriver::OnUpdateVSyncParameters(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  client_->UpdateVSyncParameters(timebase, interval);
}

}  // namespace mojo
