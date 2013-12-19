// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>


        .internal playground$segvSignalHandler
        .global playground$segvSignalHandler
playground$segvSignalHandler:
        // Inspect instruction at the point where the segmentation fault
        // happened. If it is RDTSC, forward the request to the trusted
        // thread.
        mov  $-3, %r14           // request for RDTSC
        mov  0xB0(%rsp), %r15    // %rip at time of segmentation fault
        cmpw $0x310F, (%r15)     // RDTSC
        jz   0f
        cmpw $0x010F, (%r15)     // RDTSCP
        jnz  8f
        cmpb $0xF9, 2(%r15)
        jnz  8f
        mov  $-4, %r14           // request for RDTSCP
      0:
  #ifndef NDEBUG
        lea  100f(%rip), %rdi
        call playground$debugMessage
  #endif
        sub  $4, %rsp
        push %r14
        mov  %gs:16, %edi        // fd  = threadFdPub
        mov  %rsp, %rsi          // buf = %rsp
        mov  $4, %edx            // len = sizeof(int)
      1:mov  $1, %eax            // NR_write
        syscall
        cmp  %rax, %rdx
        jz   5f
        cmp  $-4, %eax           // EINTR
        jz   1b
      2:add  $12, %rsp
        movq $0, 0x98(%rsp)      // %rax at time of segmentation fault
        movq $0, 0x90(%rsp)      // %rdx at time of segmentation fault
        cmpw $0x310F, (%r15)     // RDTSC
        jz   3f
        movq $0, 0xA0(%rsp)      // %rcx at time of segmentation fault
      3:addq $2, 0xB0(%rsp)      // %rip at time of segmentation fault
        cmpw $0x010F, (%r15)     // RDTSC
        jnz  4f
        addq $1, 0xB0(%rsp)      // %rip at time of segmentation fault
      4:ret
      5:mov  $12, %edx           // len = 3*sizeof(int)
      6:mov  $0, %eax            // NR_read
        syscall
        cmp  $-4, %eax           // EINTR
        jz   6b
        cmp  %rax, %rdx
        jnz  2b
        mov  0(%rsp), %eax
        mov  4(%rsp), %edx
        mov  8(%rsp), %ecx
        add  $12, %rsp
        mov  %rdx, 0x90(%rsp)    // %rdx at time of segmentation fault
        cmpw $0x310F, (%r15)     // RDTSC
        jz   7f
        mov  %rcx, 0xA0(%rsp)    // %rcx at time of segmentation fault
      7:mov  %rax, 0x98(%rsp)    // %rax at time of segmentation fault
        jmp  3b

        // If the instruction is INT 0, then this was probably the result
        // of playground::Library being unable to find a way to safely
        // rewrite the system call instruction. Retrieve the CPU register
        // at the time of the segmentation fault and invoke
        // syscallEntryPointWithFrame().
      8:cmpw $0x00CD, (%r15)     // INT $0x0
        jnz  16f
        cmpq $__NR_clone + 0xF001, 0x98(%rsp)
        jz   .L_handle_callback_request
  #ifndef NDEBUG
        lea  200f(%rip), %rdi
        call playground$debugMessage
  #endif
        mov  0x98(%rsp), %rax    // %rax at time of segmentation fault
        mov  0x70(%rsp), %rdi    // %rdi at time of segmentation fault
        mov  0x78(%rsp), %rsi    // %rsi at time of segmentation fault
        mov  0x90(%rsp), %rdx    // %rdx at time of segmentation fault
        mov  0x40(%rsp), %r10    // %r10 at time of segmentation fault
        mov  0x30(%rsp), %r8     // %r8  at time of segmentation fault
        mov  0x38(%rsp), %r9     // %r9  at time of segmentation fault

        // Handle rt_sigprocmask()
        cmp  $14, %rax           // NR_rt_sigprocmask
        jnz  12f
        mov  $-22, %rax          // -EINVAL
        cmp  $8, %r10            // %r10 = sigsetsize (8 bytes = 64 signals)
        jl   7b
        mov  0x130(%rsp), %r10   // signal mask at time of segmentation fault
        test %rsi, %rsi          // only set mask, if set is non-NULL
        jz   11f
        mov  0(%rsi), %rsi
        cmp  $0, %rdi            // %rdi = how (SIG_BLOCK)
        jnz  9f
        or   %rsi, 0x130(%rsp)   // signal mask at time of segmentation fault
        jmp  11f
      9:cmp  $1, %rdi            // %rdi = how (SIG_UNBLOCK)
        jnz  10f
        xor  $-1, %rsi
        and  %rsi, 0x130(%rsp)   // signal mask at time of segmentation fault
        jmp  11f
     10:cmp  $2, %rdi            // %rdi = how (SIG_SETMASK)
        jnz  7b
        mov  %rsi, 0x130(%rsp)   // signal mask at time of segmentation fault
     11:xor  %rax, %rax
        test %rdx, %rdx          // only return old mask, if set is non-NULL
        jz   7b
        mov  %r10, 0(%rdx)       // old_set
        jmp  7b

        // Handle rt_sigreturn()
     12:cmp  $15, %rax           // NR_rt_sigreturn
        jnz  14f
        mov  0xA8(%rsp), %rsp    // %rsp at time of segmentation fault
     13:syscall                  // rt_sigreturn() is unrestricted
        mov  $66, %edi           // rt_sigreturn() should never return
        mov  $231, %eax          // NR_exit_group
        jmp  13b

        // Copy signal frame onto new stack. See clone.cc for details
     14:cmp  $56+0xF000, %rax    // NR_clone + 0xF000
        jnz  15f
        lea  8(%rsp), %rax       // retain stack frame upon returning
        mov  %rax, 0xA8(%rsp)    // %rsp at time of segmentation fault
        jmp  7b

        // Forward system call to syscallEntryPointWithFrame()
     15:lea  7b(%rip), %rcx
        push %rcx
        push 0xB8(%rsp)          // %rip at time of segmentation fault
        lea  playground$syscallEntryPointWithFrame(%rip), %rcx
        jmp  *%rcx

        // In order to implement SA_NODEFER, we have to keep track of recursive
        // calls to SIGSEGV handlers. This means we have to increment a counter
        // before calling the user's signal handler, and decrement it on
        // leaving the user's signal handler.
        // Some signal handlers look at the return address of the signal
        // stack, and more importantly "gdb" uses the call to rt_sigreturn()
        // as a magic signature when doing stacktraces. So, we have to use
        // a little more unusual code to regain control after the user's
        // signal handler is done. We adjust the return address to point to
        // non-executable memory. And when we trigger another SEGV we pop the
        // extraneous signal frame and then call rt_sigreturn().
        // N.B. We currently do not correctly adjust the SEGV counter, if the
        // user's signal handler exits in way other than by returning (e.g. by
        // directly calling rt_sigreturn(), or by calling siglongjmp()).
     16:lea  22f(%rip), %r14
        cmp  %r14, %r15
        jnz  17f                 // check if returning from user's handler
        decl %gs:0x105C-0xE0     // decrement SEGV recursion counter
        mov  0xA8(%rsp), %rsp    // %rsp at time of segmentation fault
        mov  $0xF, %eax          // NR_rt_sigreturn
        syscall

        // This was a genuine segmentation fault. Check Sandbox::sa_segv_ for
        // what we are supposed to do.
     17:mov  playground$sa_segv@GOTPCREL(%rip), %rax
        cmpq $0, 0(%rax)         // SIG_DFL
        jz   18f
        cmpq $1, 0(%rax)         // SIG_IGN
        jnz  19f                 // can't really ignore synchronous signals

        // Trigger the kernel's default signal disposition. The only way we can
        // do this from seccomp mode is by blocking the signal and retriggering
        // it.
     18:orb  $4, 0x131(%rsp)     // signal mask at time of segmentation fault
        ret

        // Check sa_flags:
        //  - We can ignore SA_NOCLDSTOP, SA_NOCLDWAIT, and SA_RESTART as they
        //    do not have any effect for SIGSEGV.
        //  - On x86-64, we can also ignore SA_SIGINFO, as the calling
        //    conventions for sa_handler() are a subset of the conventions for
        //    sa_sigaction().
        //  - We have to always register our signal handler with SA_NODEFER so
        //    that the user's signal handler can make system calls which might
        //    require additional help from our SEGV handler.
        //  - If the user's signal handler wasn't supposed to be SA_NODEFER,
        //    then we emulate this behavior by keeping track of a recursion
        //    counter.
        //
        // TODO(markus): If/when we add support for sigaltstack(), we have to
        // handle SA_ONSTACK.
     19:cmpl $0, %gs:0x105C-0xE0 // check if we failed inside of SEGV handler
        jnz  18b                 // if so, then terminate program
        mov  0(%rax), %rbx       // sa_segv_.sa_sigaction
        mov  8(%rax), %rcx       // sa_segv_.sa_flags
        btl  $31, %ecx           // SA_RESETHAND
        jnc  20f
        movq $0, 0(%rax)         // set handler to SIG_DFL
     20:btl  $30, %ecx           // SA_NODEFER
        jc   21f
        mov  %r14, 0(%rsp)       // trigger a SEGV on return, so that we can
        incl %gs:0x105C-0xE0     //   clean up state; incr. recursion counter
     21:jmp  *%rbx               // call user's signal handler


        // Non-executable version of the restorer function. We use this to
        // trigger a SEGV upon returning from the user's signal handler, giving
        // us an ability to clean up prior to returning from the SEGV handler.
        .pushsection .data       // move code into non-executable section
     22:mov  $0xF, %rax          // gdb looks for this signature when doing
        syscall                  //   backtraces
        .popsection

.L_handle_callback_request:
        mov  0x90(%rsp), %rax    // %rdx at time of segmentation fault
        jmp  *%rax
