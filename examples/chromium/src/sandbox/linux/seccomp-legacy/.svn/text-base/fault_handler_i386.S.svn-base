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
        mov  $-3, %ebx           // request for RDTSC
        mov  0xDC(%esp), %ebp    // %eip at time of segmentation fault
        cmpw $0x310F, (%ebp)     // RDTSC
        jz   1f
        cmpw $0x010F, (%ebp)     // RDTSCP
        jnz  12f
        cmpb $0xF9, 2(%ebp)
        jnz  12f
        mov  $-4, %ebx           // request for RDTSCP
      1:
  #ifndef NDEBUG
        call 2f
      2:addl $(100f-.), 0(%esp)
        call playground$debugMessage@PLT
        sub  $4, %esp
  #else
        sub  $8, %esp            // allocate buffer for receiving timestamp
  #endif
        push %ebx
        mov  %fs:16, %ebx        // fd  = threadFdPub
        mov  %esp, %ecx          // buf = %esp
        mov  $4, %edx            // len = sizeof(int)
      3:mov  %edx, %eax          // NR_write
        int  $0x80
        cmp  %eax, %edx
        jz   10f
        cmp  $-4, %eax           // EINTR
        jz   3b
      4:add  $12, %esp           // remove temporary buffer from stack
        xor  %eax, %eax
        movl $0, 0xC8(%esp)      // %edx at time of segmentation fault
        cmpw $0x310F, (%ebp)     // RDTSC
        jz   5f
        movl $0, 0xCC(%esp)      // %ecx at time of segmentation fault
      5:mov  %eax, 0xD0(%esp)    // %eax at time of segmentation fault
      6:mov  0xDC(%esp), %ebp    // %eip at time of segmentation fault
        addl $2, 0xDC(%esp)      // %eip at time of segmentation fault
        cmpw $0x010F, (%ebp)     // RDTSCP
        jnz  7f
        addl $1, 0xDC(%esp)      // %eip at time of segmentation fault
      7:add  $0x4, %esp
      8:sub  $0x1CC, %esp        // a legacy signal stack is much larger
        mov  0x1CC(%esp), %eax   // push signal number
        push %eax
        lea  0x270(%esp), %esi   // copy siginfo register values
        lea  0x4(%esp), %edi     //     into new location
        mov  $22, %ecx
        cld
        rep movsl
        mov  0x2C8(%esp), %ebx   // copy first half of signal mask
        mov  %ebx, 0x54(%esp)
      9:pop  %eax                // remove dummy argument (signo)
        mov  $119, %eax          // NR_sigreturn
        int  $0x80
     10:mov  $12, %edx           // len = 3*sizeof(int)
     11:mov  $3, %eax            // NR_read
        int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   11b
        cmp  %eax, %edx
        jnz  4b
        pop  %eax
        pop  %edx
        pop  %ecx
        mov  %edx, 0xC8(%esp)    // %edx at time of segmentation fault
        cmpw $0x310F, (%ebp)     // RDTSC
        jz   5b
        mov  %ecx, 0xCC(%esp)    // %ecx at time of segmentation fault
        jmp  5b

        // If the instruction is INT 0, then this was probably the result
        // of playground::Library being unable to find a way to safely
        // rewrite the system call instruction. Retrieve the CPU register
        // at the time of the segmentation fault and invoke
        // syscallEntryPointWithFrame().
     12:cmpw $0x00CD, (%ebp)     // INT $0x0
        jnz  23f
        cmpl $__NR_clone + 0xF001, 0xD0(%esp)
        jz   .L_handle_callback_request
  #ifndef NDEBUG
        call 1010f
   1010:addl $(200f-.), 0(%esp)
        call playground$debugMessage@PLT
        add  $0x4, %esp
  #endif
        mov  0xD0(%esp), %eax    // %eax at time of segmentation fault
        mov  0xC4(%esp), %ebx    // %ebx at time of segmentation fault
        mov  0xCC(%esp), %ecx    // %ecx at time of segmentation fault
        mov  0xC8(%esp), %edx    // %edx at time of segmentation fault
        mov  0xB8(%esp), %esi    // %esi at time of segmentation fault
        mov  0xB4(%esp), %edi    // %edi at time of segmentation fault
        mov  0xBC(%esp), %ebp    // %ebp at time of segmentation fault

        // Handle sigprocmask() and rt_sigprocmask()
        cmp  $175, %eax          // NR_rt_sigprocmask
        jnz  13f
        mov  $-22, %eax          // -EINVAL
        cmp  $8, %esi            // %esi = sigsetsize (8 bytes = 64 signals)
        jl   5b
        jmp  14f
     13:cmp  $126, %eax          // NR_sigprocmask
        jnz  18f
        mov  $-22, %eax
     14:mov  0xFC(%esp), %edi    // signal mask at time of segmentation fault
        mov  0x100(%esp), %ebp
        test %ecx, %ecx          // only set mask, if set is non-NULL
        jz   17f
        mov  0(%ecx), %esi
        mov  4(%ecx), %ecx
        cmp  $0, %ebx            // %ebx = how (SIG_BLOCK)
        jnz  15f
        or   %esi, 0xFC(%esp)    // signal mask at time of segmentation fault
        or   %ecx, 0x100(%esp)
        jmp  17f
     15:cmp  $1, %ebx            // %ebx = how (SIG_UNBLOCK)
        jnz  16f
        xor  $-1, %esi
        xor  $-1, %ecx
        and  %esi, 0xFC(%esp)    // signal mask at time of segmentation fault
        and  %ecx, 0x100(%esp)
        jmp  17f
     16:cmp  $2, %ebx            // %ebx = how (SIG_SETMASK)
        jnz  5b
        mov  %esi, 0xFC(%esp)    // signal mask at time of segmentation fault
        mov  %ecx, 0x100(%esp)
     17:xor  %eax, %eax
        test %edx, %edx          // only return old mask, if set is non-NULL
        jz   5b
        mov  %edi, 0(%edx)       // old_set
        mov  %ebp, 4(%edx)
        jmp  5b

        // Handle sigreturn() and rt_sigreturn()
        // See syscall.cc for a discussion on how we can emulate rt_sigreturn()
        // by calling sigreturn() with a suitably adjusted stack.
     18:cmp  $119, %eax          // NR_sigreturn
        jnz  19f
        mov  0xC0(%esp), %esp    // %esp at time of segmentation fault
        int  $0x80               // sigreturn() is unrestricted
     19:cmp  $173, %eax          // NR_rt_sigreturn
        jnz  20f
        mov  0xC0(%esp), %esp    // %esp at time of segmentation fault
        sub  $4, %esp            // add fake return address
        jmp  6b

        // Copy signal frame onto new stack. In the process, we have to convert
        // it from an RT signal frame to a legacy signal frame.
        // See clone.cc for details
     20:cmp  $120+0xF000, %eax   // NR_clone + 0xF000
        jnz  21f
        lea  -0x1C8(%esp), %eax  // retain stack frame upon returning
        mov  %eax, 0xC0(%esp)    // %esp at time of segmentation fault
        jmp  5b

        // Forward system call to syscallEntryPointWithFrame()
     21:call 22f
     22:subl $(.-5b), 0(%esp)
        push 0xE0(%esp)          // %eip at time of segmentation fault
        jmp  playground$syscallEntryPointWithFrame@PLT

        // This was a genuine segmentation fault. Check Sandbox::sa_segv_ for
        // what we are supposed to do.
        // In order to implement SA_NODEFER, we have to keep track of recursive
        // calls to SIGSEGV handlers. This means we have to increment a counter
        // before calling the user's signal handler, and decrement it on
        // leaving the user's signal handler.
        // N.B. We currently do not correctly adjust the SEGV counter, if the
        // user's signal handler exits in way other than by returning (e.g. by
        // directly calling {,rt_}sigreturn(), or by calling siglongjmp()).
        // N.B. On i386, we don't have any guarantees that NX protection works.
        // So, we don't even attempt to fake a correct restorer function. Some
        // callers might be confused by this and will need fixing for running
        // inside of the seccomp sandbox.
     23:call 24f
     24:pop  %eax
        add  $(_GLOBAL_OFFSET_TABLE_+(.-24b)), %eax
        lea  playground$sa_segv@GOTOFF(%eax), %eax
        cmpl $0, 0(%eax)         // SIG_DFL
        jz   25f
        cmpl $1, 0(%eax)         // SIG_IGN
        jnz  26f                 // can't really ignore synchronous signals

        // Trigger the kernel's default signal disposition. The only way we can
        // do this from seccomp mode is by blocking the signal and retriggering
        // it.
     25:orb  $4, 0xFD(%esp)      // signal mask at time of segmentation fault
        jmp  7b

        // Check sa_flags:
        //  - We can ignore SA_NOCLDSTOP, SA_NOCLDWAIT, and SA_RESTART as they
        //    do not have any effect for SIGSEGV.
        //  - We have to always register our signal handler with SA_NODEFER so
        //    that the user's signal handler can make system calls which might
        //    require additional help from our SEGV handler.
        //  - If the user's signal handler wasn't supposed to be SA_NODEFER,
        //    then we emulate this behavior by keeping track of a recursion
        //    counter.
        //
        // TODO(markus): If/when we add support for sigaltstack(), we have to
        // handle SA_ONSTACK.
     26:cmpl $0, %fs:0x1040-0x58 // check if we failed inside of SEGV handler
        jnz  25b                 // if so, then terminate program
        mov  0(%eax), %ebx       // sa_segv_.sa_sigaction
        mov  4(%eax), %ecx       // sa_segv_.sa_flags
        btl  $31, %ecx           // SA_RESETHAND
        jnc  27f
        movl $0, 0(%eax)         // set handler to SIG_DFL
     27:btl  $30, %ecx           // SA_NODEFER
        jc   32f
        btl  $2, %ecx            // SA_SIGINFO
        jnc  29f
        add  $4, %esp
        call 28f
     28:addl $(35f-.), 0(%esp)   // set appropriate restorer function
        incl %fs:0x1040-0x58     // increment recursion counter
        jmp  *%ebx               // call user's signal handler
     29:add  $4, %esp
        call 30f
     30:addl $(36f-.), 0(%esp)
        incl %fs:0x1040-0x58     // increment recursion counter

        // We always register the signal handler to give us rt-style signal
        // frames. But if the user asked for legacy signal frames, we must
        // convert the signal frame prior to calling the user's signal handler.
     31:sub  $0x1C8, %esp        // a legacy signal stack is much larger
        mov  0x1CC(%esp), %eax   // push signal number
        push %eax
        mov  0x1CC(%esp), %eax   // push restorer function
        push %eax
        lea  0x274(%esp), %esi   // copy siginfo register values
        lea  0x8(%esp), %edi     //     into new location
        mov  $22, %ecx
        cld
        rep movsl
        mov  0x2CC(%esp), %eax   // copy first half of signal mask
        mov  %eax, 0x58(%esp)
        jmp  *%ebx               // call user's signal handler
     32:add  $4, %esp
        call 33f
     33:addl $(9b-.), 0(%esp)    // set appropriate restorer function
        btl  $2, %ecx            // SA_SIGINFO
        jnc  31b
        add  $4, %esp
        call 34f
     34:addl $(8b-.), 0(%esp)    // set appropriate restorer function
        jmp  *%ebx               // call user's signal handler
     35:decl %fs:0x1040-0x58
        jmp  8b
     36:decl %fs:0x1040-0x58
        jmp  9b

.L_handle_callback_request:
        mov  0xC8(%esp), %eax    // %edx at time of segmentation fault
        jmp  *%eax
