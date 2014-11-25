// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_COMMAND_BUFFER_DRIVER_H_
#define MOJO_SERVICES_GLES2_COMMAND_BUFFER_DRIVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "mojo/services/public/interfaces/gpu/command_buffer.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gpu {
class CommandBufferService;
class GpuScheduler;
class GpuControlService;
class SyncPointManager;
namespace gles2 {
class GLES2Decoder;
class MailboxManager;
}
}

namespace gfx {
class GLContext;
class GLShareGroup;
class GLSurface;
}

namespace mojo {

class CommandBufferDriver {
 public:
  class Client {
   public:
    virtual void DidDestroy() = 0;
    virtual void UpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) = 0;
    virtual void LostContext(int32_t lost_reason) = 0;

   protected:
    virtual ~Client();
  };
  // Offscreen.
  CommandBufferDriver(gfx::GLShareGroup* share_group,
                      gpu::gles2::MailboxManager* mailbox_manager,
                      gpu::SyncPointManager* sync_point_manager);
  // Onscreen.
  CommandBufferDriver(gfx::AcceleratedWidget widget,
                      const gfx::Size& size,
                      gfx::GLShareGroup* share_group,
                      gpu::gles2::MailboxManager* mailbox_manager,
                      gpu::SyncPointManager* sync_point_manager);
  ~CommandBufferDriver();

  void set_client(Client* client) { client_ = client; }

  void Initialize(CommandBufferSyncClientPtr sync_client,
                  ScopedSharedBufferHandle shared_state);
  void SetGetBuffer(int32_t buffer);
  void Flush(int32_t put_offset);
  void MakeProgress(int32_t last_get_offset);
  void RegisterTransferBuffer(int32_t id,
                              ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size);
  void DestroyTransferBuffer(int32_t id);
  void Echo(const Callback<void()>& callback);

 private:
  bool DoInitialize(ScopedSharedBufferHandle shared_state);
  void OnResize(gfx::Size size, float scale_factor);
  bool OnWaitSyncPoint(uint32_t sync_point);
  void OnSyncPointRetired();
  void OnParseError();
  void OnContextLost(uint32_t reason);
  void OnUpdateVSyncParameters(const base::TimeTicks timebase,
                               const base::TimeDelta interval);

  Client* client_;
  CommandBufferSyncClientPtr sync_client_;
  gfx::AcceleratedWidget widget_;
  gfx::Size size_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gpu::SyncPointManager> sync_point_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> context_lost_task_runner_;
  base::Callback<void(int32_t)> context_lost_callback_;

  base::WeakPtrFactory<CommandBufferDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriver);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_COMMAND_BUFFER_DRIVER_H_
