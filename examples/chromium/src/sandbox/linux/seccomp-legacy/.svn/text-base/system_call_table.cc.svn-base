// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_call_table.h"

#include <asm/unistd.h>
#include "sandbox_impl.h"

#if defined(__x86_64__)
#ifndef __NR_set_robust_list
#define __NR_set_robust_list 273
#endif
#ifndef __NR_accept4
#define __NR_accept4         288
#endif
#elif defined(__i386__)
#ifndef __NR_set_robust_list
#define __NR_set_robust_list 311
#endif
#else
#error Unsupported target platform
#endif

namespace playground {

struct SyscallTable *SyscallTable::syscallTable;
unsigned SyscallTable::maxSyscall = 0;

void SyscallTable::initializeSyscallTable() {
  if (syscallTable) {
    return;
  }

#define S(x) reinterpret_cast<void (*)()>(&Sandbox::x)
#define P(x) Sandbox::x
  static const struct policy {
    unsigned syscallNum;
    void     (*handler)();
    bool     (*trustedProcess)(const SyscallRequestInfo* info);
  } default_policy[] = {
#if defined(__NR_accept)
    { __NR_accept,          UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_accept4,         UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_access,          S(sandbox_access),          P(process_access)    },
    { __NR_brk,             UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_clock_gettime,   UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_clone,           S(sandbox_clone),           P(process_clone)     },
    { __NR_close,           UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_dup,             UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_dup2,            UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_epoll_create,    UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_epoll_ctl,       UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_epoll_wait,      UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_exit,            S(sandbox_exit),            P(process_exit)      },
    { __NR_exit_group,      UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_fcntl,           UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_fcntl64)
    { __NR_fcntl64,         UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_fdatasync,       UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_fstat,           UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_fstat64)
    { __NR_fstat64,         UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_ftruncate,       UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_futex,           UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_getdents,        UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_getdents64,      UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_getegid,         UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_getegid32)
    { __NR_getegid32,       UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_geteuid,         UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_geteuid32)
    { __NR_geteuid32,       UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_getgid,          UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_getgid32)
    { __NR_getgid32,        UNRESTRICTED_SYSCALL,       NULL                 },
#endif
#if defined(__NR_getpeername)
    { __NR_getpeername,     UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_getpid,          S(sandbox_getpid),          NULL                 },
#if defined(__NR_getsockname)
    { __NR_getsockname,     UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_getsockopt,      S(sandbox_getsockopt),      P(process_getsockopt)},
#endif
    { __NR_gettid,          S(sandbox_gettid),          NULL                 },
    { __NR_gettimeofday,    UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_getuid,          UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_getuid32)
    { __NR_getuid32,        UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_ioctl,           S(sandbox_ioctl),           P(process_ioctl)     },
#if defined(__NR_ipc)
    { __NR_ipc,             S(sandbox_ipc),             P(process_ipc)       },
#endif
#if defined(__NR__llseek)
    { __NR__llseek,         UNRESTRICTED_SYSCALL,       NULL                 },
#endif
    { __NR_lseek,           UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_lstat,           S(sandbox_lstat),           P(process_stat)      },
#if defined(__NR_lstat64)
    { __NR_lstat64,         S(sandbox_lstat64),         P(process_stat)      },
#endif
    { __NR_madvise,         S(sandbox_madvise),         P(process_madvise)   },
    {
#if defined(__NR_mmap2)
    __NR_mmap2,
#else
    __NR_mmap,
#endif
                            S(sandbox_mmap),            P(process_mmap)      },
    { __NR_mprotect,        S(sandbox_mprotect),        P(process_mprotect)  },
    { __NR_munmap,          S(sandbox_munmap),          P(process_munmap)    },
    { __NR_nanosleep,       UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_open,            S(sandbox_open),            P(process_open)      },
    { __NR_pipe,            UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_poll,            UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_prctl,           S(sandbox_prctl),           P(process_prctl)     },
    { __NR_pread64,         UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_preadv,          UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_pwrite64,        UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_pwritev,         UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_readv,           UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_recvfrom)
    { __NR_recvfrom,        S(sandbox_recvfrom),        P(process_recvfrom)  },
    { __NR_recvmsg,         S(sandbox_recvmsg),         P(process_recvmsg)   },
#endif
#if defined(__NR_rt_sigaction)
    { __NR_rt_sigaction,    S(sandbox_rt_sigaction),    P(process_sigaction) },
#endif
#if defined(__NR_rt_sigprocmask)
    { __NR_rt_sigprocmask,  S(sandbox_rt_sigprocmask),  NULL                 },
#endif
#if defined(__NR_sendmsg)
    { __NR_sendmsg,         S(sandbox_sendmsg),         P(process_sendmsg)   },
    { __NR_sendto,          S(sandbox_sendto),          P(process_sendto)    },
#endif
    { __NR_set_robust_list, UNRESTRICTED_SYSCALL,       NULL                 },
#if defined(__NR_setsockopt)
    { __NR_setsockopt,      S(sandbox_setsockopt),      P(process_setsockopt)},
#endif
#if defined(__NR_shmat)
    { __NR_shmat,           S(sandbox_shmat),           P(process_shmat)     },
    { __NR_shmctl,          S(sandbox_shmctl),          P(process_shmctl)    },
    { __NR_shmdt,           S(sandbox_shmdt),           P(process_shmdt)     },
    { __NR_shmget,          S(sandbox_shmget),          P(process_shmget)    },
#endif
#if defined(__NR_shutdown)
    { __NR_shutdown,        UNRESTRICTED_SYSCALL,       NULL                 },
#endif
#if defined(__NR_sigaction)
    { __NR_sigaction,       S(sandbox_sigaction),       P(process_sigaction) },
#endif
#if defined(__NR_signal)
    { __NR_signal,          S(sandbox_signal),          P(process_sigaction) },
#endif
#if defined(__NR_sigprocmask)
    { __NR_sigprocmask,     S(sandbox_sigprocmask),     NULL                 },
#endif
#if defined(__NR_socketpair)
    { __NR_socketpair,      UNRESTRICTED_SYSCALL,       NULL                 },
#endif
#if defined(__NR_socketcall)
    { __NR_socketcall,      S(sandbox_socketcall),      P(process_socketcall)},
#endif
    { __NR_stat,            S(sandbox_stat),            P(process_stat)      },
#if defined(__NR_stat64)
    { __NR_stat64,          S(sandbox_stat64),          P(process_stat)      },
#endif
    { __NR_time,            UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_uname,           UNRESTRICTED_SYSCALL,       NULL                 },
    { __NR_writev,          UNRESTRICTED_SYSCALL,       NULL                 },
  };

  // Find the highest used system call number
  for (const struct policy *policy = default_policy;
       policy-default_policy <
         (int)(sizeof(default_policy)/sizeof(struct policy));
       ++policy) {
    if (policy->syscallNum > maxSyscall) {
      maxSyscall = policy->syscallNum;
    }
  }

  syscallTable = reinterpret_cast<SyscallTable *>(
    mmap(NULL, getSyscallTableSize(),
         PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
  if (syscallTable == MAP_FAILED) {
    Sandbox::die("Failed to allocate system call table");
  }
  for (const struct policy *policy = default_policy;
       policy-default_policy <
         (int)(sizeof(default_policy)/sizeof(struct policy));
       ++policy) {
    syscallTable[policy->syscallNum].handler        = policy->handler;
    syscallTable[policy->syscallNum].trustedProcess = policy->trustedProcess;
  }
  protectSyscallTable();
}

size_t SyscallTable::getSyscallTableSize() {
  return ((sizeof(struct SyscallTable) * (maxSyscall + 1)) + 4095) & ~4095;
}

void SyscallTable::protectSyscallTable() {
  if (mprotect(syscallTable, getSyscallTableSize(), PROT_READ) != 0) {
    Sandbox::die("Failed to protect system call table");
  }
}

void SyscallTable::unprotectSyscallTable() {
  if (mprotect(syscallTable, getSyscallTableSize(),
               PROT_READ | PROT_WRITE) != 0) {
    Sandbox::die("Failed to unprotect system call table");
  }
}

} //  namespace
