// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_munmap(void* start, size_t length) {
  long long tm;
  Debug::syscall(&tm, __NR_munmap, "Executing handler");
  struct {
    struct RequestHeader header;
    MUnmap    munmap_req;
  } __attribute__((packed)) request;
  request.munmap_req.start  = start;
  request.munmap_req.length = length;

  long rc = forwardSyscall(__NR_munmap, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_munmap);
  return rc;
}

bool Sandbox::process_munmap(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  MUnmap munmap_req;
  if (read(sys, info->trustedProcessFd, &munmap_req, sizeof(munmap_req)) !=
      sizeof(munmap_req)) {
    die("Failed to read parameters for munmap() [process]");
  }

  // Cannot unmap any memory region that was part of the original memory
  // mappings.
  if (isRegionProtected(munmap_req.start, munmap_req.length)) {
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  // Unmapping memory regions that were newly mapped inside of the sandbox
  // is OK.
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, munmap_req.start,
                            munmap_req.length);
  return true;
}

} // namespace
