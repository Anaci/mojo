// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_DOMAIN_SOCKET_SOCKET_DESCRIPTOR_H_
#define SHELL_DOMAIN_SOCKET_SOCKET_DESCRIPTOR_H_

namespace mojo {
namespace shell {

typedef int SocketDescriptor;
const SocketDescriptor kInvalidSocket = -1;

}  // namespace mojo
}  // namespace shell

#endif  // SHELL_DOMAIN_SOCKET_SOCKET_DESCRIPTOR_H_
