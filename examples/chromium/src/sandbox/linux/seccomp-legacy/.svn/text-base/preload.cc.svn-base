// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "sandbox_impl.h"

namespace playground {

extern "C" void preload() {
  g_policy.allow_file_namespace = true;
#if defined(__x86_64__)
  // We have no guarantee about the alignment of our stack pointer when this
  // function is entered. That is a violation of the ABI. We have to explicitly
  // align it prior to calling StartSeccompSandbox().
  asm volatile("push %%rax\n"
               "lea  8(%%rsp), %%rax\n"
               "and  $-16, %%rsp\n"
               "subq $8, %%rsp\n"
               "push %%rax\n"
               "mov  -8(%%rax), %%rax\n"
               "call *%0\n"
               "mov  0(%%rsp), %%rsp\n"
               : : "r"(&StartSeccompSandbox));
#elif defined(__i386__)
  asm volatile("push %%eax\n"
               "lea  4(%%esp), %%eax\n"
               "and  $-16, %%esp\n"
               "subl $12, %%esp\n"
               "push %%eax\n"
               "mov  -4(%%eax), %%eax\n"
               "call *%0\n"
               "mov  0(%%esp), %%esp\n"
               : : "r"(&StartSeccompSandbox));
#else
  StartSeccompSandbox();
#endif
  write(2, "In secure mode, now!\n", 21);
  asm volatile("int3");

// TODO(markus): This code is just a temporary hack. Remove when not needed.
#if defined(__x86_64__)
  asm volatile(
      "sub $8, %rsp;"
      "push %rax;push %rbx;push %rcx;push %rdx;push %rbp;push %rsi;push %rdi;"
      "push %r8;push %r9;push %r10;push %r11;push %r12;push %r13;push %r14;"
      "push %r15;"
      "lea 120(%rsp), %rsi;mov $8, %rdx;mov $39, %rdi;mov $0, %rax;syscall;"
      "pop %r15;pop %r14;pop %r13;pop %r12;pop %r11;pop %r10;pop %r9;pop %r8;"
      "pop %rdi;pop %rsi;pop %rbp;pop %rdx;pop %rcx;pop %rbx;pop %rax;"
      "ret");
#endif
}

} // namespace
