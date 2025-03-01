// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/kiosk_wm/merged_service_provider.h"

namespace kiosk_wm {

MergedServiceProvider::MergedServiceProvider(
    mojo::ServiceProviderPtr exposed_services,
    mojo::InterfaceFactory<mojo::NavigatorHost>* factory)
    : exposed_services_(exposed_services.Pass()), factory_(factory) {
}

MergedServiceProvider::~MergedServiceProvider() {
}

mojo::ServiceProviderPtr MergedServiceProvider::GetServiceProviderPtr() {
  mojo::ServiceProviderPtr sp;
  binding_.reset(new mojo::Binding<mojo::ServiceProvider>(this, GetProxy(&sp)));
  return sp;
}

void MergedServiceProvider::ConnectToService(
    const mojo::String& interface_name,
    mojo::ScopedMessagePipeHandle pipe) {
  if (interface_name == mojo::NavigatorHost::Name_) {
    factory_->Create(nullptr,
                     mojo::MakeRequest<mojo::NavigatorHost>(pipe.Pass()));
  } else if (exposed_services_.get()) {
    exposed_services_->ConnectToService(interface_name, pipe.Pass());
  }
}

}  // namespace kiosk_wm
