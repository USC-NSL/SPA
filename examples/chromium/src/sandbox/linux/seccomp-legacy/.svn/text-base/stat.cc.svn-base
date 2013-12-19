// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_stat(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_stat, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    struct RequestHeader header;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc = forwardSyscall(__NR_stat, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_stat);
  return rc;
}

long Sandbox::sandbox_lstat(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_lstat, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    struct RequestHeader header;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc = forwardSyscall(__NR_lstat, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_lstat);
  return rc;
}

#if defined(__NR_stat64)
long Sandbox::sandbox_stat64(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_stat64, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    struct RequestHeader header;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc = forwardSyscall(__NR_stat64, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_stat64);
  return rc;
}

long Sandbox::sandbox_lstat64(const char *path, void *buf) {
  long long tm;
  Debug::syscall(&tm, __NR_lstat64, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    struct RequestHeader header;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc = forwardSyscall(__NR_lstat64, &request->header, sizeof(data));
  Debug::elapsed(tm, __NR_lstat64);
  return rc;
}
#endif

bool Sandbox::process_stat(const SyscallRequestInfo* info) {
  // Read request
  SysCalls sys;
  Stat stat_req;
  if (read(sys, info->trustedProcessFd, &stat_req, sizeof(stat_req)) !=
      sizeof(stat_req)) {
 read_parm_failed:
    die("Failed to read parameters for stat() [process]");
  }
  int   rc                  = -ENAMETOOLONG;
  if (stat_req.path_length >= (int)sizeof(info->mem->pathname)) {
    char buf[32];
    while (stat_req.path_length > 0) {
      size_t len            = stat_req.path_length > sizeof(buf) ?
                              sizeof(buf) : stat_req.path_length;
      ssize_t i             = read(sys, info->trustedProcessFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      stat_req.path_length -= i;
    }
    SecureMem::abandonSystemCall(*info, rc);
    return false;
  }

  if (!g_policy.allow_file_namespace) {
    // After locking the mutex, we can no longer abandon the system call. So,
    // perform checks before clobbering the securely shared memory.
    char tmp[stat_req.path_length];
    if (read(sys, info->trustedProcessFd, tmp, stat_req.path_length) !=
        (ssize_t)stat_req.path_length) {
      goto read_parm_failed;
    }
    Debug::message(("Denying access to \"" +
                    std::string(tmp, stat_req.path_length) + "\"").c_str());
    SecureMem::abandonSystemCall(*info, -EACCES);
    return false;
  }

  SecureMem::lockSystemCall(*info);
  if (read(sys, info->trustedProcessFd, info->mem->pathname,
           stat_req.path_length) != (ssize_t)stat_req.path_length) {
    goto read_parm_failed;
  }
  info->mem->pathname[stat_req.path_length] = '\000';

  Debug::message(("Allowing access to \"" + std::string(info->mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to stat the file.
  SecureMem::sendSystemCall(*info, SecureMem::SEND_LOCKED_SYNC,
                            info->mem->pathname, stat_req.buf);
  return true;
}

} // namespace
