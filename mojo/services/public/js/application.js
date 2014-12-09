// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/application", [
  "services/js/app_bridge",
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/services/public/js/service_provider",
  "mojo/services/public/js/shell",
], function(appBridgeModule, spInterfaceModule, spModule, shellModule) {

  class Application {
    constructor(appShell, url) {
      this.shell = new shellModule.Shell(appShell);
      this.url = url;
      this.serviceProviders = [];
    }

    initialize(args) {
    }

    acceptConnection_(url, spHandle) {
      var service = new spInterfaceModule.ServiceProvider.proxyClass(spHandle);
      var serviceProvider =  new spModule.ServiceProvider(service);
      this.serviceProviders.push(serviceProvider);
      this.acceptConnection(url, serviceProvider);
    }

    acceptConnection(url, serviceProvider) {
    }

    quit() {
      this.shell.close();
      this.serviceProviders.forEach(function(sp) {
        sp.close();
      });
      appBridgeModule.quit();
    }
  }

  var exports = {};
  exports.Application = Application;
  return exports;
});
