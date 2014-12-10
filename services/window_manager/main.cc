// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/common/tracing_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/view_manager/public/cpp/view_manager.h"
#include "mojo/services/view_manager/public/cpp/view_manager_delegate.h"
#include "services/window_manager/window_manager_app.h"
#include "services/window_manager/window_manager_delegate.h"

// ApplicationDelegate implementation file for WindowManager users (e.g.
// core window manager tests) that do not want to provide their own
// ApplicationDelegate::Create().

using mojo::View;
using mojo::ViewManager;

namespace window_manager {

class DefaultWindowManager : public mojo::ApplicationDelegate,
                             public mojo::ViewManagerDelegate,
                             public WindowManagerDelegate {
 public:
  DefaultWindowManager()
      : window_manager_app_(new WindowManagerApp(this, this)),
        view_manager_(NULL),
        root_(NULL) {}
  ~DefaultWindowManager() override {}

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* impl) override {
    window_manager_app_->Initialize(impl);
    mojo::TracingImpl::Create(impl);
  }
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    window_manager_app_->ConfigureIncomingConnection(connection);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(ViewManager* view_manager,
               View* root,
               mojo::ServiceProviderImpl* exported_services,
               scoped_ptr<mojo::ServiceProvider> imported_services) override {
    view_manager_ = view_manager;
    root_ = root;
  }
  void OnViewManagerDisconnected(ViewManager* view_manager) override {}

  // Overridden from WindowManagerDelegate:
  void Embed(
      const mojo::String& url,
      mojo::InterfaceRequest<mojo::ServiceProvider> service_provider) override {
    View* view = View::Create(view_manager_);
    root_->AddChild(view);
    view->SetVisible(true);
    view->Embed(url, scoped_ptr<mojo::ServiceProviderImpl>(
        new mojo::ServiceProviderImpl).Pass());
  }

  scoped_ptr<WindowManagerApp> window_manager_app_;

  ViewManager* view_manager_;
  View* root_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DefaultWindowManager);
};

}  // namespace window_manager

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(
      new window_manager::DefaultWindowManager);
  return runner.Run(shell_handle);
}
