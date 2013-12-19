// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_open(const char *pathname, int flags, mode_t mode) {
  long long tm;
  Debug::syscall(&tm, __NR_open, "Executing handler");
  size_t len                    = strlen(pathname);
  struct Request {
    struct RequestHeader header;
    Open      open_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->open_req.path_length = len;
  request->open_req.flags       = flags;
  request->open_req.mode        = mode;
  memcpy(request->pathname, pathname, len);

  long rc = forwardSyscall(__NR_open, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_open);
  return rc;
}

bool Sandbox::process_open(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  Open open_req;
  if (read(sys, info->trustedProcessFd, &open_req, sizeof(open_req)) !=
      sizeof(open_req)) {
 read_parm_failed:
    die("Failed to read parameters for open() [process]");
  }
  int   rc                  = -ENAMETOOLONG;
  if (open_req.path_length >= sizeof(info->mem->pathname)) {
    char buf[32];
    while (open_req.path_length > 0) {
      size_t len            = open_req.path_length > sizeof(buf) ?
                              sizeof(buf) : open_req.path_length;
      ssize_t i             = read(sys, info->trustedProcessFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      open_req.path_length -= i;
    }
    SecureMem::abandonSystemCall(*info, rc);
    return false;
  }

  if ((open_req.flags & O_ACCMODE) != O_RDONLY ||
      !g_policy.allow_file_namespace) {
    // After locking the mutex, we can no longer abandon the system call. So,
    // perform checks before clobbering the securely shared memory.
    char tmp[open_req.path_length];
    if (read(sys, info->trustedProcessFd, tmp, open_req.path_length) !=
        (ssize_t)open_req.path_length) {
      goto read_parm_failed;
    }
    Debug::message(("Denying access to \"" +
                    std::string(tmp, open_req.path_length) + "\"").c_str());
    SecureMem::abandonSystemCall(*info, -EACCES);
    return false;
  }

  SecureMem::lockSystemCall(*info);
  if (read(sys, info->trustedProcessFd, info->mem->pathname,
           open_req.path_length) != (ssize_t)open_req.path_length) {
    goto read_parm_failed;
  }
  info->mem->pathname[open_req.path_length] = '\000';

  Debug::message(("Allowing access to \"" + std::string(info->mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to open the file.
  SecureMem::sendSystemCall(*info, SecureMem::SEND_LOCKED_SYNC,
                            info->mem->pathname, open_req.flags, open_req.mode);
  return true;
}

} // namespace
