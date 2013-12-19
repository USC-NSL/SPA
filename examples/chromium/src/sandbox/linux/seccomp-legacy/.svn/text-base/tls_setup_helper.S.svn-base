// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>


// The function call
//   WriteAndExit(fd, buf, count);
// is equivalent to
//   write(fd, buf, count);
//   _exit(0);
// except that it will not use the stack after the write() call.
//
// This is necessary because the message sent by write() will trigger
// the creation of a new thread that reuses the stack we are called on.
// We therefore cannot call the _exit() syscall using a C function call,
// because that could write to the stack, which might overwrite data that
// the new thread is using.
//
// Although we could use a new stack for the _exit() call, we would have
// no way to deallocate it.


        .global playground$writeAndExit
playground$writeAndExit:
#if defined(__x86_64__)

        mov  $__NR_write, %rax
        // Arguments to write() are already in the correct registers.
        syscall
.L_loop:
        mov  $__NR_exit, %rax
        mov  $0, %rdi
        syscall
        jmp  .L_loop  // Just in case _exit returns.

#elif defined(__i386__)

        mov  $__NR_write, %eax
        mov  4(%esp), %ebx
        mov  8(%esp), %ecx
        mov  12(%esp), %edx
        int  $0x80
.L_loop:
        mov  $__NR_exit, %eax
        mov  $0, %ebx
        int  $0x80
        jmp  .L_loop  // Just in case _exit returns.

#else
#error Unsupported target platform
#endif

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
