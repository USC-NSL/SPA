// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


        // Performs a syscall via "int $0" given an array of syscall
        // arguments.

        .global syscall_via_int0
syscall_via_int0:
#if defined(__x86_64__)
        push %rdi
        push %rsi
        push %rdx
        push %r10
        push %r9
        push %r8
        // Set up syscall arguments
        mov 0x00(%rdi), %rax
        // Skip 0x08 (%rdi): this comes last
        mov 0x10(%rdi), %rsi
        mov 0x18(%rdi), %rdx
        mov 0x20(%rdi), %r10
        mov 0x28(%rdi), %r8
        mov 0x30(%rdi), %r9
        mov 0x08(%rdi), %rdi
        int $0
        pop %r9
        pop %r8
        pop %r10
        pop %rdx
        pop %rsi
        pop %rdi
        ret
#elif defined(__i386__)
        push %ebx
        push %ecx
        push %edx
        push %esi
        push %edi
        push %ebp
        mov 4+24(%esp), %ecx
        // Set up syscall arguments
        mov 0x00(%ecx), %eax
        mov 0x04(%ecx), %ebx
        // Skip 0x08 (%ecx): this comes last
        mov 0x0c(%ecx), %edx
        mov 0x10(%ecx), %esi
        mov 0x14(%ecx), %edi
        mov 0x18(%ecx), %ebp
        mov 0x08(%ecx), %ecx
        int $0
        pop %ebp
        pop %edi
        pop %esi
        pop %edx
        pop %ecx
        pop %ebx
        ret
#else
#error Unsupported target platform
#endif

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
