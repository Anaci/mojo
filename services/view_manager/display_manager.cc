// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/view_manager/display_manager.h"

#include "base/numerics/safe_conversions.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/gpu/interfaces/gpu.mojom.h"
#include "mojo/services/surfaces/cpp/surfaces_utils.h"
#include "mojo/services/surfaces/interfaces/quads.mojom.h"
#include "mojo/services/surfaces/interfaces/surfaces.mojom.h"
#include "services/view_manager/connection_manager.h"
#include "services/view_manager/server_view.h"
#include "services/view_manager/view_coordinate_conversions.h"

namespace view_manager {
namespace {

void DrawViewTree(mojo::Pass* pass,
                  const ServerView* view,
                  const gfx::Vector2d& parent_to_root_origin_offset,
                  float opacity) {
  if (!view->visible())
    return;

  const gfx::Rect absolute_bounds =
      view->bounds() + parent_to_root_origin_offset;
  std::vector<const ServerView*> children(view->GetChildren());
  const float combined_opacity = opacity * view->opacity();
  for (std::vector<const ServerView*>::reverse_iterator it = children.rbegin();
       it != children.rend();
       ++it) {
    DrawViewTree(pass, *it, absolute_bounds.OffsetFromOrigin(),
                 combined_opacity);
  }

  cc::SurfaceId node_id = view->surface_id();

  auto surface_quad_state = mojo::SurfaceQuadState::New();
  surface_quad_state->surface = mojo::SurfaceId::From(node_id);

  gfx::Transform node_transform;
  node_transform.Translate(absolute_bounds.x(), absolute_bounds.y());

  const gfx::Rect bounds_at_origin(view->bounds().size());
  auto surface_quad = mojo::Quad::New();
  surface_quad->material = mojo::Material::SURFACE_CONTENT;
  surface_quad->rect = mojo::Rect::From(bounds_at_origin);
  surface_quad->opaque_rect = mojo::Rect::From(bounds_at_origin);
  surface_quad->visible_rect = mojo::Rect::From(bounds_at_origin);
  surface_quad->needs_blending = true;
  surface_quad->shared_quad_state_index =
      base::saturated_cast<int32_t>(pass->shared_quad_states.size());
  surface_quad->surface_quad_state = surface_quad_state.Pass();

  auto sqs = CreateDefaultSQS(*mojo::Size::From(view->bounds().size()));
  sqs->blend_mode = mojo::SkXfermode::kSrcOver_Mode;
  sqs->opacity = combined_opacity;
  sqs->content_to_target_transform = mojo::Transform::From(node_transform);

  pass->quads.push_back(surface_quad.Pass());
  pass->shared_quad_states.push_back(sqs.Pass());
}

}  // namespace

DefaultDisplayManager::DefaultDisplayManager(
    mojo::ApplicationImpl* app_impl,
    mojo::ApplicationConnection* app_connection,
    const mojo::Closure& native_viewport_closed_callback)
    : app_impl_(app_impl),
      app_connection_(app_connection),
      connection_manager_(nullptr),
      draw_timer_(false, false),
      frame_pending_(false),
      native_viewport_closed_callback_(native_viewport_closed_callback),
      weak_factory_(this) {
  metrics_.size = mojo::Size::New();
  metrics_.size->width = 800;
  metrics_.size->height = 600;
}

void DefaultDisplayManager::Init(ConnectionManager* connection_manager) {
  connection_manager_ = connection_manager;
  app_impl_->ConnectToService("mojo:native_viewport_service",
                              &native_viewport_);
  // The connection error handler will be called if native_viewport_ is torn
  // down before this object is destroyed.
  native_viewport_.set_connection_error_handler(
      [this]() { native_viewport_closed_callback_.Run(); });
  native_viewport_->Create(metrics_.size->Clone(),
                           mojo::SurfaceConfiguration::New(),
                           base::Bind(&DefaultDisplayManager::OnMetricsChanged,
                                      weak_factory_.GetWeakPtr()));
  native_viewport_->Show();

  mojo::ContextProviderPtr context_provider;
  native_viewport_->GetContextProvider(GetProxy(&context_provider));
  mojo::DisplayFactoryPtr display_factory;
  app_impl_->ConnectToService("mojo:surfaces_service", &display_factory);
  display_factory->Create(context_provider.Pass(),
                          nullptr,  // returner - we never submit resources.
                          GetProxy(&display_));

  mojo::NativeViewportEventDispatcherPtr event_dispatcher;
  app_connection_->ConnectToService(&event_dispatcher);
  native_viewport_->SetEventDispatcher(event_dispatcher.Pass());
}

DefaultDisplayManager::~DefaultDisplayManager() {
}

void DefaultDisplayManager::SchedulePaint(const ServerView* view,
                                          const gfx::Rect& bounds) {
  if (!view->IsDrawn(connection_manager_->root()))
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenViews(view, connection_manager_->root(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  WantToDraw();
}

void DefaultDisplayManager::SetViewportSize(const gfx::Size& size) {
  native_viewport_->SetSize(mojo::Size::From(size));
}

const mojo::ViewportMetrics& DefaultDisplayManager::GetViewportMetrics() {
  return metrics_;
}

void DefaultDisplayManager::Draw() {
  mojo::Rect rect;
  rect.width = metrics_.size->width;
  rect.height = metrics_.size->height;
  auto pass = CreateDefaultPass(1, rect);
  pass->damage_rect = mojo::Rect::From(dirty_rect_);

  DrawViewTree(pass.get(), connection_manager_->root(), gfx::Vector2d(), 1.0f);

  auto frame = mojo::Frame::New();
  frame->passes.push_back(pass.Pass());
  frame->resources.resize(0u);
  frame_pending_ = true;
  display_->SubmitFrame(
      frame.Pass(),
      base::Bind(&DefaultDisplayManager::DidDraw, base::Unretained(this)));
  dirty_rect_ = gfx::Rect();
}

void DefaultDisplayManager::DidDraw() {
  frame_pending_ = false;
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

void DefaultDisplayManager::WantToDraw() {
  if (draw_timer_.IsRunning() || frame_pending_)
    return;

  draw_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&DefaultDisplayManager::Draw, base::Unretained(this)));
}

void DefaultDisplayManager::OnMetricsChanged(mojo::ViewportMetricsPtr metrics) {
  metrics_.size = metrics->size.Clone();
  metrics_.device_pixel_ratio = metrics->device_pixel_ratio;
  gfx::Rect bounds(metrics_.size.To<gfx::Size>());
  connection_manager_->root()->SetBounds(bounds);
  connection_manager_->ProcessViewportMetricsChanged(metrics_, *metrics);
  native_viewport_->RequestMetrics(base::Bind(
      &DefaultDisplayManager::OnMetricsChanged, weak_factory_.GetWeakPtr()));
}

}  // namespace view_manager
