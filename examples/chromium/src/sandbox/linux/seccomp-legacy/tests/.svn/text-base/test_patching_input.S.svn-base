// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>


        // This performs a system call directly so that we can test
        // patching this instruction sequence.

        .global my_getpid
        .global my_getpid_end
my_getpid:
        mov $__NR_getpid, %eax
#if defined(__x86_64__)
        syscall
#elif defined(__i386__)
        int $0x80
#else
# error Unsupported target platform
#endif
        ret
my_getpid_end:


        // These routines call the vsyscall page, which is present on
        // x86-64 only.

#if defined(__x86_64__)

        .global my_vgettimeofday
        .global my_vgettimeofday_end
my_vgettimeofday:
        sub $8, %rsp  // Align the stack
        mov $0xffffffffff600000, %rax
        call *%rax
        add $8, %rsp
        ret
my_vgettimeofday_end:

        .global my_vtime
        .global my_vtime_end
my_vtime:
        sub $8, %rsp  // Align the stack
        mov $0xffffffffff600400, %rax
        call *%rax
        add $8, %rsp
        ret
my_vtime_end:

        .global my_vgetcpu
        .global my_vgetcpu_end
my_vgetcpu:
        sub $8, %rsp  // Align the stack
        mov $0xffffffffff600800, %rax
        call *%rax
        add $8, %rsp
        ret
my_vgetcpu_end:

#endif

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
