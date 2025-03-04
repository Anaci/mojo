// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SKIA_GANESH_TEXTURE_SURFACE_H_
#define MOJO_SKIA_GANESH_TEXTURE_SURFACE_H_

#include <memory>

#include "mojo/gpu/gl_texture.h"
#include "mojo/skia/ganesh_context.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace mojo {

// This class represents an SkSurface backed by a GL texture, which is
// appropriate for use with Ganesh.  This is useful for rendering Skia
// commands to a texture.
class GaneshTextureSurface {
 public:
  // Creates a surface that wraps the specified GL texture.
  GaneshTextureSurface(GaneshContext* context,
                       std::unique_ptr<GLTexture> texture);
  ~GaneshTextureSurface();

  SkSurface* surface() const { return surface_.get(); }
  SkCanvas* canvas() const { return surface_->getCanvas(); }

  // Destroys the surface and returns its underlying texture.
  std::unique_ptr<GLTexture> TakeTexture();

 private:
  std::unique_ptr<GLTexture> texture_;
  skia::RefPtr<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(GaneshTextureSurface);
};

}  // namespace mojo

#endif  // MOJO_SKIA_GANESH_TEXTURE_SURFACE_H_
