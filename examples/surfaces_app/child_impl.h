// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_SURFACES_APP_CHILD_IMPL_H_
#define EXAMPLES_SURFACES_APP_CHILD_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "examples/surfaces_app/child.mojom.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/surfaces/interfaces/surface_id.mojom.h"
#include "mojo/services/surfaces/interfaces/surfaces.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
}

namespace mojo {

class ApplicationConnection;

namespace examples {

// Simple example of a child app using surfaces.
class ChildImpl : public Child {
 public:
  class Context {
   public:
    virtual ApplicationConnection* ShellConnection(
        const mojo::String& application_url) = 0;
  };
  ChildImpl(ApplicationConnection* surfaces_service_connection,
            InterfaceRequest<Child> request);
  ~ChildImpl() override;

 private:
  using ProduceCallback = mojo::Callback<void(SurfaceIdPtr id)>;

  void SetIdNamespace(uint32_t id_namespace);

  // Child implementation.
  void ProduceFrame(ColorPtr color,
                    SizePtr size,
                    const ProduceCallback& callback) override;

  scoped_ptr<cc::SurfaceIdAllocator> allocator_;
  SurfacePtr surface_;
  uint32_t id_namespace_;
  StrongBinding<Child> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChildImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // EXAMPLES_SURFACES_APP_CHILD_IMPL_H_
