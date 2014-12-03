// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/gles2/gpu_impl.h"

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "services/gles2/command_buffer_driver.h"
#include "services/gles2/command_buffer_impl.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace mojo {

GpuImpl::State::State()
    : control_thread_("gpu_command_buffer_control"),
      sync_point_manager_(new gpu::SyncPointManager),
      share_group_(new gfx::GLShareGroup),
      mailbox_manager_(new gpu::gles2::MailboxManagerImpl) {
  control_thread_.Start();
}

GpuImpl::State::~State() {
}

GpuImpl::GpuImpl(InterfaceRequest<Gpu> request,
                 const scoped_refptr<State>& state)
    : binding_(this, request.Pass()), state_(state) {
}

GpuImpl::~GpuImpl() {
}

void GpuImpl::CreateOnscreenGLES2Context(
    uint64_t native_viewport_id,
    SizePtr size,
    InterfaceRequest<CommandBuffer> request,
    ViewportParameterListenerPtr listener) {
  gfx::AcceleratedWidget widget = bit_cast<gfx::AcceleratedWidget>(
      static_cast<uintptr_t>(native_viewport_id));
  new CommandBufferImpl(
      request.Pass(), listener.Pass(), state_->control_task_runner(),
      state_->sync_point_manager(),
      make_scoped_ptr(new CommandBufferDriver(
          widget, size.To<gfx::Size>(), state_->share_group(),
          state_->mailbox_manager(), state_->sync_point_manager())));
}

void GpuImpl::CreateOffscreenGLES2Context(
    InterfaceRequest<CommandBuffer> request) {
  new CommandBufferImpl(request.Pass(), ViewportParameterListenerPtr(),
                        state_->control_task_runner(),
                        state_->sync_point_manager(),
                        make_scoped_ptr(new CommandBufferDriver(
                            state_->share_group(), state_->mailbox_manager(),
                            state_->sync_point_manager())));
}

}  // namespace mojo
