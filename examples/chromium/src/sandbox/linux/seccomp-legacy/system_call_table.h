// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSCALL_TABLE_H__
#define SYSCALL_TABLE_H__

#include <sys/types.h>
#include "securemem.h"

#ifndef INTERNAL
#define INTERNAL __attribute__((visibility("internal")))
#endif

namespace playground {

typedef SecureMem::Args SecureMemArgs;
typedef SecureMem::SyscallRequestInfo SyscallRequestInfo;
#define UNRESTRICTED_SYSCALL ((void (*)())1)

struct SyscallTable {
  void  (*handler)();
  bool  (*trustedProcess)(const SyscallRequestInfo* info);

  // Initializes the system call table with the default policy
  static void initializeSyscallTable();

  static size_t getSyscallTableSize();
  static void protectSyscallTable();
  static void unprotectSyscallTable();

  // The pointers to the system call table are r/w. This is OK, as we create
  // a r/o copy in the secure memory page(s) just prior to launching the
  // sandbox. The trusted thread then only ever access the table through
  // its secure memory.
  // The table itself (as opposed to the pointer to it) always is marked
  // as r/o.
  static unsigned maxSyscall
    asm("playground$maxSyscall") INTERNAL;
  static struct SyscallTable *syscallTable
    asm("playground$syscallTable") INTERNAL;
};

} // namespace

#endif // SYSCALL_TABLE_H__
