// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

void* Sandbox::sandbox_mmap(void *start, size_t length, int prot, int flags,
                          int fd, off_t offset) {
  long long tm;
  Debug::syscall(&tm, __NR_mmap, "Executing handler");
  struct {
    struct RequestHeader header;
    MMap      mmap_req;
  } __attribute__((packed)) request;
  request.mmap_req.start  = start;
  request.mmap_req.length = length;
  request.mmap_req.prot   = prot;
  request.mmap_req.flags  = flags;
  request.mmap_req.fd     = fd;
  request.mmap_req.offset = offset;

  long rc = forwardSyscall(__NR_MMAP, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_mmap);
  return (void *) rc;
}

bool Sandbox::process_mmap(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  MMap mmap_req;
  if (read(sys, info->trustedProcessFd, &mmap_req, sizeof(mmap_req)) !=
      sizeof(mmap_req)) {
    die("Failed to read parameters for mmap() [process]");
  }

  // Cannot map a memory area that was part of the original memory mappings.
  if ((mmap_req.flags & MAP_FIXED) != 0 &&
      isRegionProtected(mmap_req.start, mmap_req.length)) {
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  // All other mmap() requests are OK
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, mmap_req.start,
                            mmap_req.length, mmap_req.prot, mmap_req.flags,
                            mmap_req.fd, mmap_req.offset);
  return true;
}

} // namespace
