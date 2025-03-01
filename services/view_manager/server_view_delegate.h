// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
#define SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_

#include "mojo/services/view_manager/interfaces/view_manager_constants.mojom.h"

namespace gfx {
class Rect;
}

namespace mojo {
class ViewportMetrics;
}

namespace view_manager {

class ServerView;

// ServerViewDelegate is notified at key points in the lifetime of a
// ServerView. Some of the functions are similar to that of
// ServerViewObserver. For example, ServerViewDelegate::PrepareToDestroyView()
// and ServerViewObserver::OnWillDestroyView(). The key difference between
// the two are the ServerViewDelegate ones are always notified first, and
// ServerViewDelegate gets non-const arguments.
class ServerViewDelegate {
 public:
  // Invoked when a view is about to be destroyed; before any of the children
  // have been removed and before the view has been removed from its parent.
  virtual void PrepareToDestroyView(ServerView* view) = 0;

  virtual void PrepareToChangeViewHierarchy(ServerView* view,
                                            ServerView* new_parent,
                                            ServerView* old_parent) = 0;

  virtual void PrepareToChangeViewVisibility(ServerView* view) = 0;

  virtual void OnScheduleViewPaint(const ServerView* view) = 0;

 protected:
  virtual ~ServerViewDelegate() {}
};

}  // namespace view_manager

#endif  // SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
