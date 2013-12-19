// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_access(const char *pathname, int mode) {
  long long tm;
  Debug::syscall(&tm, __NR_access, "Executing handler");
  size_t len                      = strlen(pathname);
  struct Request {
    struct RequestHeader header;
    Access    access_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                         = reinterpret_cast<struct Request*>(data);
  request->access_req.path_length = len;
  request->access_req.mode        = mode;
  memcpy(request->pathname, pathname, len);

  long rc = forwardSyscall(__NR_access, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_access);
  return rc;
}

bool Sandbox::process_access(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  Access access_req;
  if (read(sys, info->trustedProcessFd, &access_req, sizeof(access_req)) !=
      sizeof(access_req)) {
 read_parm_failed:
    die("Failed to read parameters for access() [process]");
  }
  int   rc                    = -ENAMETOOLONG;
  if (access_req.path_length >= sizeof(info->mem->pathname)) {
    char buf[32];
    while (access_req.path_length > 0) {
      size_t len              = access_req.path_length > sizeof(buf) ?
                                sizeof(buf) : access_req.path_length;
      ssize_t i               = read(sys, info->trustedProcessFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      access_req.path_length -= i;
    }
    if (write(sys, info->applicationFd, &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to return data from access() [process]");
    }
    return false;
  }

  if (!g_policy.allow_file_namespace) {
    // After locking the mutex, we can no longer abandon the system call. So,
    // perform checks before clobbering the securely shared memory.
    char tmp[access_req.path_length];
    if (read(sys, info->trustedProcessFd, tmp, access_req.path_length) !=
        (ssize_t)access_req.path_length) {
      goto read_parm_failed;
    }
    Debug::message(("Denying access to \"" +
                    std::string(tmp, access_req.path_length) + "\"").c_str());
    SecureMem::abandonSystemCall(*info, -EACCES);
    return false;
  }

  SecureMem::lockSystemCall(*info);
  if (read(sys, info->trustedProcessFd, info->mem->pathname,
           access_req.path_length) != (ssize_t)access_req.path_length) {
    goto read_parm_failed;
  }
  info->mem->pathname[access_req.path_length] = '\000';

  // TODO(markus): Implement sandboxing policy
  Debug::message(("Allowing access to \"" + std::string(info->mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to access the file.
  SecureMem::sendSystemCall(*info, SecureMem::SEND_LOCKED_SYNC,
                            info->mem->pathname, access_req.mode);
  return true;
}

} // namespace
