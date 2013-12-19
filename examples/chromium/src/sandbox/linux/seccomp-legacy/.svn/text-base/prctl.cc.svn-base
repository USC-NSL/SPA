// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_prctl(int option, unsigned long arg2, unsigned long arg3,
                            unsigned long arg4, unsigned long arg5) {
  long long tm;
  Debug::syscall(&tm, __NR_prctl, "Executing handler");
  struct Request {
    struct RequestHeader header;
    Prctl     prctl_req;
  } __attribute__((packed)) request;
  request.prctl_req.option = option;
  request.prctl_req.arg2   = arg2;
  request.prctl_req.arg3   = arg3;
  request.prctl_req.arg4   = arg4;
  request.prctl_req.arg5   = arg5;

  long rc = forwardSyscall(__NR_prctl, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_prctl);
  return rc;
}

bool Sandbox::process_prctl(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  Prctl prctl_req;
  if (read(sys, info->trustedProcessFd, &prctl_req, sizeof(prctl_req)) !=
      sizeof(prctl_req)) {
    die("Failed to read parameters for prctl() [process]");
  }
  switch (prctl_req.option) {
  case PR_GET_DUMPABLE:
  case PR_SET_DUMPABLE:
    SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, prctl_req.option,
                              prctl_req.arg2, 0, 0, 0);
    return true;
  default:
    SecureMem::abandonSystemCall(*info, -EPERM);
    return false;
  }
}

} // namespace
