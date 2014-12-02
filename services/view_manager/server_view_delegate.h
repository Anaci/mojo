// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
#define SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_

namespace gfx {
class Rect;
}

namespace mojo {
namespace service {

class ServerView;

class ServerViewDelegate {
 public:
  // Invoked when a view is about to be destroyed; before any of the children
  // have been removed and before the view has been removed from its parent.
  virtual void OnWillDestroyView(ServerView* view) = 0;

  // Invoked at the end of the View's destructor (after it has been removed from
  // the hierarchy).
  virtual void OnViewDestroyed(const ServerView* view) = 0;

  virtual void OnWillChangeViewHierarchy(ServerView* view,
                                         ServerView* new_parent,
                                         ServerView* old_parent) = 0;

  virtual void OnViewHierarchyChanged(const ServerView* view,
                                      const ServerView* new_parent,
                                      const ServerView* old_parent) = 0;

  virtual void OnViewBoundsChanged(const ServerView* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) = 0;

  virtual void OnViewSurfaceIdChanged(const ServerView* view) = 0;

  virtual void OnViewReordered(const ServerView* view,
                               const ServerView* relative,
                               OrderDirection direction) = 0;

  virtual void OnWillChangeViewVisibility(ServerView* view) = 0;

  virtual void OnViewSharedPropertyChanged(
      const ServerView* view,
      const std::string& name,
      const std::vector<uint8_t>* new_data) = 0;

  virtual void OnScheduleViewPaint(const ServerView* view) = 0;

 protected:
  virtual ~ServerViewDelegate() {}
};

}  // namespace service
}  // namespace mojo

#endif  // SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
