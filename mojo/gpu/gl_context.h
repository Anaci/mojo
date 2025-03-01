// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GPU_GL_CONTEXT_H_
#define MOJO_GPU_GL_CONTEXT_H_

#include <MGL/mgl.h>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace mojo {
class CommandBuffer;
using CommandBufferPtr = InterfacePtr<CommandBuffer>;
class MojoGLES2Impl;
class Shell;

class GLContext {
 public:
  class Observer {
   public:
    virtual void OnContextLost() = 0;

   protected:
    virtual ~Observer();
  };

  static base::WeakPtr<GLContext> Create(Shell* shell);
  static base::WeakPtr<GLContext> CreateFromCommandBuffer(
      CommandBufferPtr command_buffer);

  void MakeCurrent();
  void Destroy();

  gpu::gles2::GLES2Interface* gl() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  explicit GLContext(CommandBufferPtr command_buffer);
  ~GLContext();

  static void ContextLostThunk(void* self);
  void OnContextLost();

  MGLContext context_;
  scoped_ptr<MojoGLES2Impl> gl_impl_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<GLContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

}  // namespace mojo

#endif  // MOJO_GPU_GL_CONTEXT_H_
