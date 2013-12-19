// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>

// This is used for testing that the clone() syscall preserves all registers.

#if defined(__x86_64__)

        .global clone_test_helper
clone_test_helper:
        // Save all general purpose registers except %rax.
        push %rbx
        push %rcx
        push %rdx
        push %rsi
        push %rdi
        push %rbp
        push %r8
        push %r9
        push %r10
        push %r11
        push %r12
        push %r13
        push %r14
        push %r15
        mov  g_input_regs + 8*0(%rip), %rax
        mov  g_input_regs + 8*1(%rip), %rbx
        mov  g_input_regs + 8*2(%rip), %rcx
        mov  g_input_regs + 8*3(%rip), %rdx
        mov  g_input_regs + 8*4(%rip), %rsi
        mov  g_input_regs + 8*5(%rip), %rdi
        mov  g_input_regs + 8*6(%rip), %rbp
        mov  g_input_regs + 8*7(%rip), %r8
        mov  g_input_regs + 8*8(%rip), %r9
        mov  g_input_regs + 8*9(%rip), %r10
        mov  g_input_regs + 8*10(%rip), %r11
        mov  g_input_regs + 8*11(%rip), %r12
        mov  g_input_regs + 8*12(%rip), %r13
        mov  g_input_regs + 8*13(%rip), %r14
        mov  g_input_regs + 8*14(%rip), %r15
        call playground$syscallEntryPointNoFrame
        cmp  $0, %rax
        jz child_thread
        mov  %rax, g_out_regs_parent + 8*0(%rip)
        mov  %rbx, g_out_regs_parent + 8*1(%rip)
        mov  %rcx, g_out_regs_parent + 8*2(%rip)
        mov  %rdx, g_out_regs_parent + 8*3(%rip)
        mov  %rsi, g_out_regs_parent + 8*4(%rip)
        mov  %rdi, g_out_regs_parent + 8*5(%rip)
        mov  %rbp, g_out_regs_parent + 8*6(%rip)
        mov  %r8,  g_out_regs_parent + 8*7(%rip)
        mov  %r9,  g_out_regs_parent + 8*8(%rip)
        mov  %r10, g_out_regs_parent + 8*9(%rip)
        mov  %r11, g_out_regs_parent + 8*10(%rip)
        mov  %r12, g_out_regs_parent + 8*11(%rip)
        mov  %r13, g_out_regs_parent + 8*12(%rip)
        mov  %r14, g_out_regs_parent + 8*13(%rip)
        mov  %r15, g_out_regs_parent + 8*14(%rip)
        // Restore all general purpose registers except %rax.
        pop %r15
        pop %r14
        pop %r13
        pop %r12
        pop %r11
        pop %r10
        pop %r9
        pop %r8
        pop %rbp
        pop %rdi
        pop %rsi
        pop %rdx
        pop %rcx
        pop %rbx
        ret
child_thread:
        mov  %rax, g_out_regs_child + 8*0(%rip)
        mov  %rbx, g_out_regs_child + 8*1(%rip)
        mov  %rcx, g_out_regs_child + 8*2(%rip)
        mov  %rdx, g_out_regs_child + 8*3(%rip)
        mov  %rsi, g_out_regs_child + 8*4(%rip)
        mov  %rdi, g_out_regs_child + 8*5(%rip)
        mov  %rbp, g_out_regs_child + 8*6(%rip)
        mov  %r8,  g_out_regs_child + 8*7(%rip)
        mov  %r9,  g_out_regs_child + 8*8(%rip)
        mov  %r10, g_out_regs_child + 8*9(%rip)
        mov  %r11, g_out_regs_child + 8*10(%rip)
        mov  %r12, g_out_regs_child + 8*11(%rip)
        mov  %r13, g_out_regs_child + 8*12(%rip)
        mov  %r14, g_out_regs_child + 8*13(%rip)
        mov  %r15, g_out_regs_child + 8*14(%rip)
        mov  $__NR_exit, %rax
        mov  $0, %rdi
        syscall
        hlt

#elif defined(__i386__)

        .global clone_test_helper
clone_test_helper:
        // Save all general purpose registers except %eax.
        push %ebx
        push %ecx
        push %edx
        push %esi
        push %edi
        push %ebp
        mov  g_input_regs + 4*0, %eax
        mov  g_input_regs + 4*1, %ebx
        mov  g_input_regs + 4*2, %ecx
        mov  g_input_regs + 4*3, %edx
        mov  g_input_regs + 4*4, %esi
        mov  g_input_regs + 4*5, %edi
        mov  g_input_regs + 4*6, %ebp
        call playground$syscallEntryPointNoFrame
        cmp  $0, %eax
        jz   child_thread
        mov  %eax, g_out_regs_parent + 4*0
        mov  %ebx, g_out_regs_parent + 4*1
        mov  %ecx, g_out_regs_parent + 4*2
        mov  %edx, g_out_regs_parent + 4*3
        mov  %esi, g_out_regs_parent + 4*4
        mov  %edi, g_out_regs_parent + 4*5
        mov  %ebp, g_out_regs_parent + 4*6
        // Restore all general purpose registers except %eax.
        pop  %ebp
        pop  %edi
        pop  %esi
        pop  %edx
        pop  %ecx
        pop  %ebx
        ret
child_thread:
        mov  %eax, g_out_regs_child + 4*0
        mov  %ebx, g_out_regs_child + 4*1
        mov  %ecx, g_out_regs_child + 4*2
        mov  %edx, g_out_regs_child + 4*3
        mov  %esi, g_out_regs_child + 4*4
        mov  %edi, g_out_regs_child + 4*5
        mov  %ebp, g_out_regs_child + 4*6
        mov  $__NR_exit, %eax
        mov  $0, %ebx
        int  $0x80
        hlt

#else
#error Unsupported target platform
#endif

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
