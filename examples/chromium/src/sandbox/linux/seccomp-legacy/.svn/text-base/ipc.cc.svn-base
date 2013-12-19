// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

#ifndef IPC_PRIVATE
#define IPC_PRIVATE 0
#endif
#ifndef IPC_RMID
#define IPC_RMID    0
#endif
#ifndef IPC_64
#define IPC_64      256
#endif

#if defined(__NR_shmget)
void* Sandbox::sandbox_shmat(int shmid, const void* shmaddr, int shmflg) {
  long long tm;
  Debug::syscall(&tm, __NR_shmat, "Executing handler");

  struct {
    struct RequestHeader header;
    ShmAt     shmat_req;
  } __attribute__((packed)) request;
  request.shmat_req.shmid    = shmid;
  request.shmat_req.shmaddr  = shmaddr;
  request.shmat_req.shmflg   = shmflg;

  long rc = forwardSyscall(__NR_shmat, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_shmat);
  return reinterpret_cast<void *>(rc);
}

long Sandbox::sandbox_shmctl(int shmid, int cmd, void* buf) {
  long long tm;
  Debug::syscall(&tm, __NR_shmctl, "Executing handler");

  struct {
    struct RequestHeader header;
    ShmCtl    shmctl_req;
  } __attribute__((packed)) request;
  request.shmctl_req.shmid = shmid;
  request.shmctl_req.cmd   = cmd;
  request.shmctl_req.buf   = buf;

  long rc = forwardSyscall(__NR_shmctl, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_shmctl);
  return rc;
}

long Sandbox::sandbox_shmdt(const void* shmaddr) {
  long long tm;
  Debug::syscall(&tm, __NR_shmdt, "Executing handler");

  struct {
    struct RequestHeader header;
    ShmDt     shmdt_req;
  } __attribute__((packed)) request;
  request.shmdt_req.shmaddr  = shmaddr;

  long rc = forwardSyscall(__NR_shmdt, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_shmdt);
  return rc;
}

long Sandbox::sandbox_shmget(int key, size_t size, int shmflg) {
  long long tm;
  Debug::syscall(&tm, __NR_shmget, "Executing handler");

  struct {
    struct RequestHeader header;
    ShmGet    shmget_req;
  } __attribute__((packed)) request;
  request.shmget_req.key    = key;
  request.shmget_req.size   = size;
  request.shmget_req.shmflg = shmflg;

  long rc = forwardSyscall(__NR_shmget, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_shmget);
  return rc;
}

bool Sandbox::process_shmat(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  ShmAt shmat_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &shmat_req, sizeof(shmat_req)) !=
      sizeof(shmat_req)) {
    die("Failed to read parameters for shmat() [process]");
  }

  // We only allow attaching to the shm identifier that was returned by
  // the most recent call to shmget(IPC_PRIVATE)
  if (!g_policy.unrestricted_sysv_mem &&
      (shmat_req.shmaddr || shmat_req.shmflg ||
       shmat_req.shmid != info->mem->shmId)) {
    info->mem->shmId = -1;
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  info->mem->shmId   = -1;
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, shmat_req.shmid,
                            const_cast<void*>(shmat_req.shmaddr),
                            shmat_req.shmflg);
  return true;
}

bool Sandbox::process_shmctl(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  ShmCtl shmctl_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &shmctl_req, sizeof(shmctl_req)) !=
      sizeof(shmctl_req)) {
    die("Failed to read parameters for shmctl() [process]");
  }

  // The only shmctl() operation that we need to support is removal. This
  // operation is generally safe.
  if (!g_policy.unrestricted_sysv_mem &&
      ((shmctl_req.cmd & ~(IPC_64 | IPC_RMID)) || shmctl_req.buf)) {
    info->mem->shmId = -1;
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  info->mem->shmId   = -1;
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, shmctl_req.shmid,
                            shmctl_req.cmd, shmctl_req.buf);
  return true;
}

bool Sandbox::process_shmdt(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  ShmDt shmdt_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &shmdt_req, sizeof(shmdt_req)) !=
      sizeof(shmdt_req)) {
    die("Failed to read parameters for shmdt() [process]");
  }

  // Detaching shared memory segments it generally safe, but just in case
  // of a kernel bug, we make sure that the address does not fall into any
  // of the reserved memory regions.
  if (!g_policy.unrestricted_sysv_mem &&
      isRegionProtected((void *) shmdt_req.shmaddr, 0x1000)) {
    info->mem->shmId = -1;
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  info->mem->shmId     = -1;
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED,
                            const_cast<void*>(shmdt_req.shmaddr));
  return true;
}

bool Sandbox::process_shmget(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  ShmGet shmget_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &shmget_req, sizeof(shmget_req)) !=
      sizeof(shmget_req)) {
    die("Failed to read parameters for shmget() [process]");
  }

  // We do not want to allow the sandboxed application to access arbitrary
  // shared memory regions. We only allow it to access regions that it
  // created itself.
  if (!g_policy.unrestricted_sysv_mem &&
      (shmget_req.key != IPC_PRIVATE || shmget_req.shmflg & ~0777)) {
    info->mem->shmId = -1;
    SecureMem::abandonSystemCall(*info, -EINVAL);
    return false;
  }

  info->mem->shmId   = -1;
  SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, shmget_req.key,
                            shmget_req.size, shmget_req.shmflg);
  return true;
}
#endif

#if defined(__NR_ipc)
long Sandbox::sandbox_ipc(unsigned call, int first, int second, int third,
                         void* ptr, long fifth) {
  long long tm;
  Debug::syscall(&tm, __NR_ipc, "Executing handler", call);
  struct {
    struct RequestHeader header;
    IPC       ipc_req;
  } __attribute__((packed)) request;
  request.ipc_req.call   = call;
  request.ipc_req.first  = first;
  request.ipc_req.second = second;
  request.ipc_req.third  = third;
  request.ipc_req.ptr    = ptr;
  request.ipc_req.fifth  = fifth;

  long rc = forwardSyscall(__NR_ipc, &request.header, sizeof(request));
  Debug::elapsed(tm, __NR_ipc, call);
  return rc;
}

bool Sandbox::process_ipc(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  IPC ipc_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &ipc_req, sizeof(ipc_req)) !=
      sizeof(ipc_req)) {
    die("Failed to read parameters for ipc() [process]");
  }

  // We do not support all of the SysV IPC calls. In fact, we only support
  // the minimum feature set necessary for Chrome's renderers to share memory
  // with the X server.
  if (g_policy.unrestricted_sysv_mem) {
    goto accept;
  }
  switch (ipc_req.call) {
    case SHMAT: {
      // We only allow attaching to the shm identifier that was returned by
      // the most recent call to shmget(IPC_PRIVATE)
      if (ipc_req.ptr || ipc_req.second || ipc_req.first != info->mem->shmId) {
        goto deny;
      }
    accept:
      info->mem->shmId = -1;
      SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED, ipc_req.call,
                                ipc_req.first, ipc_req.second, ipc_req.third,
                                ipc_req.ptr, ipc_req.fifth);
      return true;
    }
    case SHMCTL:
      // The only shmctl() operation that we need to support is removal. This
      // operation is generally safe.
      if ((ipc_req.second & ~(IPC_64 | IPC_RMID)) || ipc_req.ptr) {
        goto deny;
      } else {
        goto accept;
      }
    case SHMDT: {
      // Detaching shared memory segments it generally safe, but just in case
      // of a kernel bug, we make sure that the address does not fall into any
      // of the reserved memory regions.
      if (isRegionProtected(ipc_req.ptr, 0x1000)) {
        goto deny;
      } else {
        goto accept;
      }
    }
    case SHMGET:
      // We do not want to allow the sandboxed application to access arbitrary
      // shared memory regions. We only allow it to access regions that it
      // created itself.
      if (ipc_req.first != IPC_PRIVATE || ipc_req.third & ~0777) {
        goto deny;
      } else {
        goto accept;
      }
    default:
      // Other than SysV shared memory, we do not actually need to support any
      // other SysV IPC calls.
    deny:
      info->mem->shmId = -1;
      SecureMem::abandonSystemCall(*info, -EINVAL);
      return false;
  }
}
#endif

} // namespace
