// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_URL_RESPONSE_DISK_CACHE_LOADER_H_
#define SHELL_URL_RESPONSE_DISK_CACHE_LOADER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "services/url_response_disk_cache/url_response_disk_cache_app.h"
#include "shell/application_manager/application_loader.h"

namespace shell {

class URLResponseDiskCacheLoader : public ApplicationLoader {
 public:
  URLResponseDiskCacheLoader();
  ~URLResponseDiskCacheLoader() override;

 private:
  // ApplicationLoader overrides:
  void Load(
      const GURL& url,
      mojo::InterfaceRequest<mojo::Application> application_request) override;

  mojo::URLResponseDiskCacheApp url_response_disk_cache_;
  scoped_ptr<mojo::ApplicationImpl> application_;

  DISALLOW_COPY_AND_ASSIGN(URLResponseDiskCacheLoader);
};

}  // namespace shell

#endif  // SHELL_URL_RESPONSE_DISK_CACHE_LOADER_H_
