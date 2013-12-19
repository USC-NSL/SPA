// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "securemem.h"

#include "debug.h"
#include "mutex.h"

namespace playground {

void SecureMem::abandonSystemCall(const SyscallRequestInfo& rpc, long err) {
  void* rc = reinterpret_cast<void *>(err);
  if (err) {
    Debug::message("System call failed\n");
  }
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, rpc.applicationFd, &rc, sizeof(rc)) != sizeof(rc)) {
    Sandbox::die("Failed to send system call");
  }
}

void SecureMem::dieIfParentDied(int parentMapsFd) {
  // syscallMutex should not stay locked for long.  If it is, then either:
  //  1) We are executing a blocking syscall, and the sandbox should be
  //     changed not to wait for syscallMutex while the syscall is blocking.
  //  2) The sandboxed process terminated while the trusted process was in
  //     the middle of waiting for the mutex.  We detect this situation and
  //     terminate the trusted process.
  int alive = !lseek(parentMapsFd, 0, SEEK_SET);
  if (alive) {
    char buf;
    do {
      alive = read(parentMapsFd, &buf, 1);
    } while (alive < 0 && errno == EINTR);
  }
  if (!alive) {
    Sandbox::die();
  }
}

void SecureMem::lockSystemCall(const SyscallRequestInfo& rpc) {
  while (!Mutex::lockMutex(&rpc.mem->syscallMutex, 500)) {
    dieIfParentDied(rpc.parentMapsFd);
  }
  asm volatile(
  #if defined(__x86_64__)
      "lock; incq (%0)\n"
  #elif defined(__i386__)
      "lock; incl (%0)\n"
  #else
  #error Unsupported target platform
  #endif
      :
      : "q"(&rpc.mem->sequence)
      : "memory");
}

void SecureMem::sendSystemCallInternal(const SyscallRequestInfo& rpc,
                                       LockType type,
                                       void* arg1, void* arg2, void* arg3,
                                       void* arg4, void* arg5, void* arg6,
                                       Args* newSecureMem) {
  if (type == SEND_UNLOCKED) {
    asm volatile(
    #if defined(__x86_64__)
        "lock; incq (%0)\n"
    #elif defined(__i386__)
        "lock; incl (%0)\n"
    #else
    #error Unsupported target platform
    #endif
        :
        : "q"(&rpc.mem->sequence)
        : "memory");
  }
  rpc.mem->callType    = type == SEND_UNLOCKED ? -1 : -2;
  rpc.mem->syscallNum  = rpc.sysnum;
  rpc.mem->arg1        = arg1;
  rpc.mem->arg2        = arg2;
  rpc.mem->arg3        = arg3;
  rpc.mem->arg4        = arg4;
  rpc.mem->arg5        = arg5;
  rpc.mem->arg6        = arg6;
  rpc.mem->newSecureMem = newSecureMem;
  asm volatile(
  #if defined(__x86_64__)
      "lock; incq (%0)\n"
  #elif defined(__i386__)
      "lock; incl (%0)\n"
  #else
  #error Unsupported target platform
  #endif
      :
      : "q"(&rpc.mem->sequence)
      : "memory");
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, rpc.trustedThreadFd, &rpc.mem->callType,
                     sizeof(int)) != sizeof(int)) {
    Sandbox::die("Failed to send system call");
  }
  if (type == SEND_LOCKED_SYNC) {
    while (!Mutex::waitForUnlock(&rpc.mem->syscallMutex, 500)) {
      dieIfParentDied(rpc.parentMapsFd);
    }
  }
}

} // namespace
