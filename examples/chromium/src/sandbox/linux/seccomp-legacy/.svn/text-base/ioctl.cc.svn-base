// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_ioctl(int d, int req, void *arg) {
  long long tm;
  Debug::syscall(&tm, __NR_ioctl, "Executing handler");
  struct {
    struct RequestHeader header;
    IOCtl     ioctl_req;
  } __attribute__((packed)) request;
  request.ioctl_req.d   = d;
  request.ioctl_req.req = req;
  request.ioctl_req.arg = arg;

  long rc = forwardSyscall(__NR_ioctl, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_ioctl);
  return rc;
}

bool Sandbox::process_ioctl(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  IOCtl ioctl_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &ioctl_req, sizeof(ioctl_req)) !=
      sizeof(ioctl_req)) {
    die("Failed to read parameters for ioctl() [process]");
  }
  int rc = -EINVAL;
  switch (ioctl_req.req) {
    case TCGETS:
    case TIOCGWINSZ:
      SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, ioctl_req.d,
                                ioctl_req.req, ioctl_req.arg);
      return true;
    default:
      if (Debug::isEnabled()) {
        char buf[80];
        sprintf(buf, "Unsupported ioctl: 0x%04X\n", ioctl_req.req);
        Debug::message(buf);
      }
      SecureMem::abandonSystemCall(*info, rc);
      return false;
  }
}

} // namespace
