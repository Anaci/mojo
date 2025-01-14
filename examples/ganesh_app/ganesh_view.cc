// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/ganesh_app/ganesh_view.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "mojo/skia/ganesh_texture_surface.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"

namespace examples {

namespace {

mojo::Size ToSize(const mojo::Rect& rect) {
  mojo::Size size;
  size.width = rect.width;
  size.height = rect.height;
  return size;
}

}  // namespace

GaneshView::GaneshView(mojo::Shell* shell, mojo::View* view)
    : view_(view),
      gl_context_(mojo::GLContext::Create(shell)),
      gr_context_(new mojo::GaneshContext(gl_context_)),
      texture_uploader_(this, shell, gl_context_) {
  view_->AddObserver(this);
  Draw(ToSize(view_->bounds()));
}

GaneshView::~GaneshView() {
  if (gl_context_) {
    // GaneshContext needs to be destroyed before GLContext.
    gr_context_.reset();
    gl_context_->Destroy();
  }
}

void GaneshView::OnSurfaceIdAvailable(mojo::SurfaceIdPtr surface_id) {
  view_->SetSurfaceId(surface_id.Pass());
}

void GaneshView::OnViewDestroyed(mojo::View* view) {
  DCHECK(view == view_);
  view_->RemoveObserver(this);
  delete this;
}

void GaneshView::OnViewInputEvent(mojo::View* view,
                                  const mojo::EventPtr& event) {
  Draw(ToSize(view_->bounds()));
}

void GaneshView::OnViewBoundsChanged(mojo::View* view,
                                     const mojo::Rect& old_bounds,
                                     const mojo::Rect& new_bounds) {
  Draw(ToSize(new_bounds));
}

void GaneshView::Draw(const mojo::Size& size) {
  TRACE_EVENT0("ganesh_app", __func__);
  mojo::GaneshContext::Scope scope(gr_context_.get());
  mojo::GaneshTextureSurface surface(
      gr_context_.get(),
      std::unique_ptr<mojo::GLTexture>(new mojo::GLTexture(gl_context_, size)));

  SkCanvas* canvas = surface.canvas();
  canvas->clear(SK_ColorCYAN);

  SkPaint paint;
  paint.setColor(SK_ColorGREEN);
  SkRect rect = SkRect::MakeWH(size.width, size.height);
  rect.inset(10, 10);
  canvas->drawRect(rect, paint);

  paint.setColor(SK_ColorRED);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  canvas->drawCircle(50, 100, 100, paint);

  canvas->flush();

  texture_uploader_.Upload(
      scoped_ptr<mojo::GLTexture>(surface.TakeTexture().release()));
}

}  // namespace examples
