// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/window_manager/native_viewport_event_dispatcher_impl.h"

#include "mojo/converters/input_events/input_events_type_converters.h"
#include "services/window_manager/view_event_dispatcher.h"
#include "services/window_manager/window_manager_app.h"

namespace mojo {

NativeViewportEventDispatcherImpl::NativeViewportEventDispatcherImpl(
    WindowManagerApp* app)
    : app_(app) {
}
NativeViewportEventDispatcherImpl::~NativeViewportEventDispatcherImpl() {
}

ui::EventProcessor* NativeViewportEventDispatcherImpl::GetEventProcessor() {
  return app_->event_dispatcher();
}

void NativeViewportEventDispatcherImpl::OnEvent(
    mojo::EventPtr event,
    const mojo::Callback<void()>& callback) {
  scoped_ptr<ui::Event> ui_event = event.To<scoped_ptr<ui::Event>>();

  if (ui_event)
    SendEventToProcessor(ui_event.get());

  callback.Run();
}

}  // namespace mojo
