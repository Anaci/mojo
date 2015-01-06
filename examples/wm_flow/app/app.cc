// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "examples/bitmap_uploader/bitmap_uploader.h"
#include "examples/wm_flow/app/embedder.mojom.h"
#include "examples/wm_flow/embedded/embeddee.mojom.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/view_manager/public/cpp/view.h"
#include "mojo/services/view_manager/public/cpp/view_manager.h"
#include "mojo/services/view_manager/public/cpp/view_manager_client_factory.h"
#include "mojo/services/view_manager/public/cpp/view_manager_context.h"
#include "mojo/services/view_manager/public/cpp/view_manager_delegate.h"
#include "mojo/services/view_manager/public/cpp/view_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace examples {
namespace {

const SkColor kColors[] = { SK_ColorRED, SK_ColorGREEN, SK_ColorYELLOW };

class EmbedderImpl : public mojo::InterfaceImpl<Embedder> {
 public:
  EmbedderImpl() {}
  virtual ~EmbedderImpl() {}

 private:
  // Overridden from Embedder:
  virtual void HelloWorld(const mojo::Callback<void()>& callback) override {
    callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(EmbedderImpl);
};

}  // namespace

// This app starts its life via Connect() rather than by being embed, so it does
// not start with a connection to the ViewManager service. It has to obtain a
// connection by connecting to the ViewManagerInit service and asking to be
// embed without a view context.
class WMFlowApp : public mojo::ApplicationDelegate,
                  public mojo::ViewManagerDelegate,
                  public mojo::ViewObserver {
 public:
  WMFlowApp() : shell_(nullptr), embed_count_(0) {}
  virtual ~WMFlowApp() { STLDeleteValues(&uploaders_); }

 private:
  typedef std::map<mojo::View*, mojo::BitmapUploader*> ViewToUploader;

  // Overridden from Application:
  virtual void Initialize(mojo::ApplicationImpl* app) override {
    shell_ = app->shell();
    view_manager_client_factory_.reset(
        new mojo::ViewManagerClientFactory(app->shell(), this));
    view_manager_context_.reset(new mojo::ViewManagerContext(app));
    // FIXME: Mojo applications don't know their URLs yet:
    // https://docs.google.com/a/chromium.org/document/d/1AQ2y6ekzvbdaMF5WrUQmneyXJnke-MnYYL4Gz1AKDos
    url_ = GURL(app->args()[1]);
    OpenNewWindow();
    OpenNewWindow();
    OpenNewWindow();
  }
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  void OnConnect(bool success) {}

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::View* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> imported_services) override {
    root->AddObserver(this);
    mojo::BitmapUploader* uploader = new mojo::BitmapUploader(root);
    uploaders_[root] = uploader;
    uploader->Init(shell_);
    // BitmapUploader does not track view size changes, we would
    // need to subscribe to OnViewBoundsChanged and tell the bitmap uploader
    // to invalidate itself.  This is better done once if had a per-view
    // object instead of holding per-view state on the ApplicationDelegate.
    uploader->SetColor(kColors[embed_count_++ % arraysize(kColors)]);

    mojo::View* embed = root->view_manager()->CreateView();
    root->AddChild(embed);
    mojo::Rect bounds;
    bounds.x = bounds.y = 25;
    bounds.width = root->bounds().width - 50;
    bounds.height = root->bounds().height - 50;
    embed->SetBounds(bounds);
    embed->SetVisible(true);

    scoped_ptr<mojo::ServiceProviderImpl> registry(
        new mojo::ServiceProviderImpl);
    // Expose some services to the embeddee...
    registry->AddService(&embedder_factory_);

    GURL embedded_app_url = url_.Resolve("wm_flow_embedded.mojo");
    scoped_ptr<mojo::ServiceProvider> imported =
        embed->Embed(embedded_app_url.spec(), registry.Pass());
    // FIXME: This is wrong.  We're storing per-view state on this per-app
    // delegate.  Every time a new view is created embedee_ is replaced
    // causing the previous one to shut down.  This class should not
    // be a ViewManagerDelegate, but rather a separate object should be
    // created for each embedding.
    mojo::ConnectToService(imported.get(), &embeddee_);
    embeddee_->HelloBack(base::Bind(&WMFlowApp::HelloBackAck,
                                    base::Unretained(this)));
  }
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) override {
    STLDeleteValues(&uploaders_);
  }

  // Overridden from mojo::ViewObserver:
  virtual void OnViewInputEvent(mojo::View* view,
                                const mojo::EventPtr& event) override {
    if (event->action == mojo::EVENT_TYPE_MOUSE_RELEASED &&
        event->flags & mojo::EVENT_FLAGS_LEFT_MOUSE_BUTTON) {
      OpenNewWindow();
    }
  }
  virtual void OnViewDestroyed(mojo::View* view) override {
    if (uploaders_.find(view) != uploaders_.end()) {
      delete uploaders_[view];
      uploaders_.erase(view);
    }
    --embed_count_;
    view->RemoveObserver(this);
  }

  void HelloBackAck() {
    printf("HelloBack() ack'ed\n");
  }

  void OpenNewWindow() { view_manager_context_->Embed(url_.spec()); }

  mojo::Shell* shell_;
  int embed_count_;
  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;
  mojo::InterfaceFactoryImpl<EmbedderImpl> embedder_factory_;
  scoped_ptr<mojo::ViewManagerContext> view_manager_context_;
  EmbeddeePtr embeddee_;
  ViewToUploader uploaders_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WMFlowApp);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::WMFlowApp);
  return runner.Run(shell_handle);
}
