// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>


#define CHECK_SYSCALL_ZERO  test %eax, %eax; jnz fatal_error


        .internal playground$runTrustedThread
        .global playground$runTrustedThread
playground$runTrustedThread:
        mov  4(%esp), %edi       // 1st arg: SecureMem::Args*
        mov  8(%esp), %esi       // 2nd arg: segment selector for %fs

        push %ebx
        push %ebp

        // Signal handlers are process-wide. This means that for security
        // reasons, we cannot allow that the trusted thread ever executes any
        // signal handlers.
        // We prevent the execution of signal handlers by setting a signal
        // mask that blocks all signals. In addition, we make sure that the
        // stack pointer is invalid.
        // We cannot reset the signal mask until after we have enabled
        // Seccomp mode. Our sigprocmask() wrapper would normally do this by
        // raising a signal, modifying the signal mask in the kernel-generated
        // signal frame, and then calling sigreturn(). This presents a bit of
        // a Catch-22, as all signals are masked and we can therefore not
        // raise any signal that would allow us to generate the signal stack
        // frame.
        // Instead, we have to create the signal stack frame prior to entering
        // Seccomp mode. This incidentally also helps us to restore the
        // signal mask to the same value that it had prior to entering the
        // sandbox.
        // The signal wrapper for clone() is the second entry point into this
        // code (by means of sending an IPC to its trusted thread). It goes
        // through the same steps of creating a signal stack frame on the
        // newly created thread's stacks prior to cloning. See clone.cc for
        // details.
        mov  $__NR_clone + 0xF000, %eax
        mov  %esp, %ebp
        int  $0                  // push a signal stack frame (see clone.cc)
        movw %si, %fs
        mov  %esi, 0x4(%esp)     // set up %fs upon call to sigreturn()
        mov  %ebp, 0x1C(%esp)    // pop stack upon call to sigreturn()
        call 0f                  // determine %eip for PIC addressing
      0:pop  %ebp
        movd %ebp, %mm1
        add  $(_GLOBAL_OFFSET_TABLE_+(.-0b)), %ebp
        mov  playground$cloneFdPub@GOT(%ebp), %ebp
        movd 0(%ebp), %mm3
        movd %mm1, %ebp
        add  $(999f-0b), %ebp
        mov  %ebp, 0x38(%esp)    // return address: continue in same thread
        mov  %esp, %ebp
        mov  $2, %ebx            // how     = SIG_SETMASK
        pushl $-1
        pushl $-1
        mov  %esp, %ecx          // set     = full mask
        xor  %edx, %edx          // old_set = NULL
        mov  $8, %esi            // mask all 64 signals
        mov  $__NR_rt_sigprocmask, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        mov  $__NR_sigprocmask, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        xor  %esp, %esp          // invalidate the stack in all trusted code
        movd %edi, %mm6          // %mm6 = args
        xor  %edi, %edi          // initial sequence number
        movd %edi, %mm2
        jmp  20f                 // create trusted thread

        // TODO(markus): Coalesce the read() operations by reading into a
        //               bigger buffer.

        // Parameters:
        //   %mm0: thread's side of threadFd
        //   %mm1: base address used for position independent code
        //   %mm3: cloneFdPub
        //   %mm5: secure memory region
        //         the page following this one contains the scratch space

        // Local variables:
        //   %mm2: sequence number for trusted calls
        //   %mm4: thread id

        // Temporary variables:
        //   %ebp: system call number
        //   %mm6: secure memory of previous thread
        //   %mm7: temporary variable for spilling data

        // Layout of secure shared memory region (c.f. securemem.h):
        //   0x00: pointer to the secure shared memory region (i.e. self)
        //   0x04: sequence number; must match %mm2
        //   0x08: call type; must match %eax, iff %eax == -1 || %eax == -2
        //   0x0C: system call number; passed to syscall in %eax
        //   0x10: first argument; passed to syscall in %ebx
        //   0x14: second argument; passed to syscall in %ecx
        //   0x18: third argument; passed to syscall in %edx
        //   0x1C: fourth argument; passed to syscall in %esi
        //   0x20: fifth argument; passed to syscall in %edi
        //   0x24: sixth argument; passed to syscall in %ebp
        //   0x28-0x40: no longer used
        //   0x44: new shared memory for clone()
        //   0x48: no longer used
        //   0x4C: no longer used
        //   0x50: set to non-zero, if in debugging mode
        //   0x54: most recent SHM id returned by shmget(IPC_PRIVATE)
        //   0x58: cookie assigned to us by the trusted process (TLS_COOKIE)
        //   0x60: thread id (TLS_TID)
        //   0x68: threadFdPub (TLS_THREAD_FD)
        //   0x70: syscallMutex
        //   0x74: maxSyscall
        //   0x78: syscallTable
        //   0x200-0x1000: securely passed verified file name(s)

        // Layout of (untrusted) scratch space:
        //   0x00: syscall number; passed in %eax
        //   0x04: first argument; passed in %ebx
        //   0x08: second argument; passed in %ecx
        //   0x0C: third argument; passed in %edx
        //   0x10: fourth argument; passed in %esi
        //   0x14: fifth argument; passed in %edi
        //   0x18: sixth argument; passed in %ebp
        //   0x1C: return value
        //   0x20: RDTSCP result (%eax)
        //   0x24: RDTSCP result (%edx)
        //   0x28: RDTSCP result (%ecx)
        //   0x2C: last system call (updated in syscall.cc)
        //   0x30: number of consecutive calls to a time fnc (e.g.gettimeofday)
        //   0x34: nesting level of system calls (for debugging purposes only)
        //   0x38: signal mask
        //   0x40: in SEGV handler

      1:xor  %esp, %esp
        mov  $2, %eax            // %mm2 = initial sequence number
        movd %eax, %mm2

        // Read request from untrusted thread, or from trusted process. In
        // either case, the data that we read has to be considered untrusted.
        // read(threadFd, &scratch, 4)
      2:mov  $__NR_read, %eax
        movd %mm0, %ebx          // fd  = threadFd
        movd %mm5, %ecx          // secure_mem
        add  $0x1000, %ecx       // buf = &scratch
        mov  $4, %edx            // len = 4
      3:int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   3b
        cmp  %edx, %eax
        jnz  fatal_error

        // Retrieve system call number. It is crucial that we only dereference
        // 0x1000(%mm5) exactly once. Afterwards, memory becomes untrusted and
        // we must use the value that we have read the first time.
        mov  0(%ecx), %eax

        // If syscall number is -1, execute an unlocked system call from the
        // secure memory area
        cmp  $-1, %eax
        jnz  5f
        movd %mm2, %ebp
        cmp  %ebp, 0x4-0x1000(%ecx)
        jne  fatal_error
        cmp  0x08-0x1000(%ecx), %eax
        jne  fatal_error
        mov  0x0C-0x1000(%ecx), %eax
        mov  0x10-0x1000(%ecx), %ebx
        mov  0x18-0x1000(%ecx), %edx
        mov  0x1C-0x1000(%ecx), %esi
        mov  0x20-0x1000(%ecx), %edi
        mov  0x24-0x1000(%ecx), %ebp
        mov  0x14-0x1000(%ecx), %ecx
        movd %edi, %mm4
        movd %ebp, %mm7
        movd %mm2, %ebp
        movd %mm5, %edi
        cmp  %ebp, 4(%edi)
        jne  fatal_error
        add  $2, %ebp
        movd %ebp, %mm2
        movd %mm4, %edi
        movd %mm7, %ebp

        // clone() has unusual calling conventions and must be handled
        // specially
        cmp  $__NR_clone, %eax
        jz   19f

        // shmget() gets some special treatment. Whenever we return from this
        // system call, we remember the most recently returned SysV shm id.
        cmp  $__NR_ipc, %eax
        jnz  4f
        cmp  $23, %ebx           // shmget()
        jnz  4f
        int  $0x80
        mov  %eax, %ebp
        mov  $__NR_clone, %eax
        mov  $17, %ebx           // flags = SIGCHLD
        mov  $1, %ecx            // stack = 1
        int  $0x80
        test %eax, %eax
        js   fatal_error
        mov  %eax, %ebx
        jnz  8f                  // wait for child, then return result
        movd %mm5, %ebx          // start = secure_mem
        mov  $4096, %ecx         // len   = 4096
        mov  $3, %edx            // prot  = PROT_READ | PROT_WRITE
        mov  $__NR_mprotect, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        mov  %ebp, 0x54(%ebx)    // set most recently returned SysV shm id
        xor  %ebx, %ebx

        // When debugging messages are enabled, warn about expensive system
        // calls
        #ifndef NDEBUG
        movd %mm5, %ecx
        cmpw $0, 0x50(%ecx)      // debug mode
        jz   26f
        mov  $__NR_write, %eax
        mov  $2, %ebx            // fd = stderr
        movd %mm1, %ecx
        add  $(101f-0b), %ecx    // "This is an expensive system call"
        mov  $102f-101f, %edx    // len = strlen(msg)
        int  $0x80
        xor  %ebx, %ebx
        #endif

        jmp  26f                 // exit program, no message
      4:int  $0x80
        jmp  15f                 // return result

        // If syscall number is -2, execute locked system call from the
        // secure memory area
      5:jg   12f
        cmp  $-2, %eax
        jnz  9f
        movd %mm2, %ebp
        cmp  %ebp, 0x4-0x1000(%ecx)
        jne  fatal_error
        cmp  %eax, 0x8-0x1000(%ecx)
        jne  fatal_error

        // When debugging messages are enabled, warn about expensive system
        // calls
        #ifndef NDEBUG
        cmpw $0, 0x50-0x1000(%ecx)
        jz   6f                  // debug mode
        mov  %ecx, %ebp
        mov  $__NR_write, %eax
        mov  $2, %ebx            // fd = stderr
        movd %mm1, %ecx
        add  $(101f-0b), %ecx    // "This is an expensive system call"
        mov  $102f-101f, %edx    // len = strlen(msg)
        int  $0x80
        mov  %ebp, %ecx
     6:
        #endif

        mov  0x0C-0x1000(%ecx), %eax
        mov  0x10-0x1000(%ecx), %ebx
        mov  0x18-0x1000(%ecx), %edx
        mov  0x1C-0x1000(%ecx), %esi
        mov  0x20-0x1000(%ecx), %edi
        mov  0x24-0x1000(%ecx), %ebp
        mov  0x14-0x1000(%ecx), %ecx
        movd %edi, %mm4
        movd %ebp, %mm7
        movd %mm2, %ebp
        movd %mm5, %edi
        cmp  %ebp, 4(%edi)
        jne  fatal_error

        // exit() terminates trusted thread
        cmp  $__NR_exit, %eax
        jz   18f

        // Perform requested system call
        movd %mm4, %edi
        movd %mm7, %ebp
        int  $0x80

        // Unlock mutex
      7:movd %mm2, %ebp
        movd %mm5, %edi
        cmp  %ebp, 4(%edi)
        jne  fatal_error
        add  $2, %ebp
        movd %ebp, %mm2
        mov  %eax, %ebp
        mov  $__NR_clone, %eax
        mov  $17, %ebx           // flags = SIGCHLD
        mov  $1, %ecx            // stack = 1
        int  $0x80
        test %eax, %eax
        js   fatal_error
        jz   22f                 // unlock and exit
        mov  %eax, %ebx
      8:xor  %ecx, %ecx
        xor  %edx, %edx
        mov  $__NR_waitpid, %eax
        int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   8b
        mov  %ebp, %eax
        jmp  15f                 // return result

        // If syscall number is -3, read the time stamp counter
      9:cmp  $-3, %eax
        jnz  10f
        rdtsc                    // sets %edx:%eax
        xor  %ecx, %ecx
        jmp  11f
     10:cmp  $-4, %eax
        jnz  12f
        rdtscp                   // sets %edx:%eax and %ecx
     11:movd %mm5, %ebx
        add  $0x1020, %ebx
        mov  %eax, 0(%ebx)
        mov  %edx, 4(%ebx)
        mov  %ecx, 8(%ebx)
        mov  %ebx, %ecx
        mov  $12, %edx
        jmp  16f                 // return result

        // Check in syscallTable whether this system call is unrestricted
     12:mov  %eax, %ebp
        #ifndef NDEBUG
        cmpw $0, 0x50-0x1000(%ecx)
        jnz  13f                 // debug mode
        #endif
        movd %mm5, %ebx
        cmp  0x74(%ebx), %eax    // maxSyscall
        ja   fatal_error
        shl  $3, %eax
        add  0x78(%ebx), %eax    // syscallTable
        mov  0(%eax), %eax
        cmp  $1, %eax
        jne  fatal_error

        // Default behavior for unrestricted system calls is to just execute
        // them. Read the remaining arguments first.
     13:mov  $__NR_read, %eax
        movd %mm0, %ebx          // fd  = threadFd
        add  $4, %ecx            // buf = &scratch + 4
        mov  $24, %edx           // len = 6*sizeof(void *)
     14:int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   14b
        cmp  %edx, %eax
        jnz  fatal_error
        mov  %ebp, %eax
        mov  0x00(%ecx), %ebx
        mov  0x08(%ecx), %edx
        mov  0x0C(%ecx), %esi
        mov  0x10(%ecx), %edi
        mov  0x14(%ecx), %ebp
        mov  0x04(%ecx), %ecx
        cmp  $__NR_exit_group, %eax
        jz   26f                 // exit program, no message
        int  $0x80

        // Return result of system call to sandboxed thread
     15:movd %mm5, %ecx          // secure_mem
        add  $0x101C, %ecx       // buf   = &scratch + 28
        mov  %eax, (%ecx)
        mov  $4, %edx            // len   = 4
     16:movd %mm0, %ebx          // fd    = threadFd
        mov  $__NR_write, %eax
     17:int  $0x80
        cmp  %edx, %eax
        jz   2b
        cmp  $-4, %eax           // EINTR
        jz   17b
        jmp  fatal_error

        // NR_exit:
        // Exit trusted thread after cleaning up resources
     18:mov  %edi, %ecx          // secure_mem
        mov  0x68(%ecx), %ebx    // fd     = threadFdPub
        mov  $__NR_close, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        mov  %ecx, %ebx          // start  = secure_mem
        mov  $8192, %ecx         // length = 8192
        xor  %edx, %edx          // prot   = PROT_NONE
        mov  $__NR_mprotect, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        movd %mm0, %ebx          // fd     = threadFd
        mov  $__NR_close, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        mov  $__NR_clone, %eax
        mov  $17, %ebx           // flags = SIGCHLD
        mov  $1, %ecx            // stack = 1
        int  $0x80
        mov  %eax, %ebx
        test %eax, %eax
        js   fatal_error
        jne  21f                 // reap helper, exit thread
        jmp  22f                 // unlock mutex

        // NR_clone:
        // Original trusted thread calls clone() to create new nascent
        // thread. This thread is (typically) fully privileged and shares all
        // resources with the caller (i.e. the previous trusted thread),
        // and by extension it shares all resources with the sandbox'd
        // threads.
     19:movd %mm5, %edi
        movd %edi, %mm6          // %mm6  = old_shared_mem
        movd %mm4, %edi          // child_tidptr
        mov  %ecx, %ebp          // remember child stack
        mov  $1, %ecx            // stack = 1
        int  $0x80               // calls NR_clone
        cmp  $-4095, %eax        // return codes -1..-4095 are errno values
        jae  7b                  // unlock mutex, return result
        test %eax, %eax
        jne  15b                 // return result

        // In nascent thread, now.
        // Undo sequence number increase that was made for the general case.
        movd %mm2, %edi
        sub  $2, %edi
        movd %edi, %mm2

        // We want to maintain an invalid %esp whenver we access untrusted
        // memory. This ensures that even if an attacker can trick us into
        // triggering a SIGSEGV, we will never successfully execute a signal
        // handler.
        // Signal handlers are inherently dangerous, as an attacker could trick
        // us into returning to the wrong address by adjusting the signal stack
        // right before the handler returns.
        // N.B. While POSIX is curiously silent about this, it appears that on
        // Linux, alternate signal stacks are a per-thread property. That is
        // good. It means that this security mechanism works, even if the
        // sandboxed thread manages to set up an alternate signal stack.
        //
        // TODO(markus): We currently do not support emulating calls to
        // sys_clone() with a zero (i.e. copy) stack parameter. See clone.cc
        // for a discussion on how to fix this, if this ever becomes neccessary

        // Get thread id of nascent thread
     20:mov  $__NR_gettid, %eax
        int  $0x80
        movd %eax, %mm4

        // Nascent thread creates socketpair() for sending requests to
        // trusted thread.
        // We can create the filehandles on the child's stack. Filehandles are
        // always treated as untrusted.
        // socketpair(AF_UNIX, SOCK_STREAM, 0, fds)
        mov  $__NR_socketcall, %eax
        mov  $8, %ebx            // socketpair
        sub  $8, %ebp            // sv       = child_stack
        mov  %ebp, -0x04(%ebp)
        movl $0, -0x08(%ebp)     // protocol = 0
        movl $1, -0x0C(%ebp)     // type     = SOCK_STREAM
        movl $1, -0x10(%ebp)     // domain   = AF_UNIX
        lea  -0x10(%ebp), %ecx
        int  $0x80
        test %eax, %eax
        jz   27f

        // If things went wrong, we don't have an (easy) way of signaling
        // the parent. For our purposes, it is sufficient to fail with a
        // fatal error.
        jmp  fatal_error
     21:xor  %ecx, %ecx
        xor  %edx, %edx
        mov  $__NR_waitpid, %eax
        int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   21b
        jmp  23f                 // exit thread (no message)
        // Unlock syscallMutex and exit.
     22:movd %mm5, %ebx
        mov  $4096, %ecx
        mov  $3, %edx            // prot = PROT_READ | PROT_WRITE
        mov  $__NR_mprotect, %eax
        int  $0x80
        CHECK_SYSCALL_ZERO
        addl $0x70, %ebx
        lock; addl $0x80000000, (%ebx)
        jz   23f                 // exit thread
        mov  $1, %edx
        mov  %edx, %ecx          // FUTEX_WAKE
        mov  $__NR_futex, %eax
        int  $0x80
     23:mov  $__NR_exit, %eax
        mov  $1, %ebx            // status = 1
     24:int  $0x80
fatal_error:
        mov  $__NR_write, %eax
        mov  $2, %ebx            // fd = stderr
        movd %mm1, %ecx
        add  $(100f-0b), %ecx    // "Sandbox violation detected"
        mov  $101f-100f, %edx    // len = strlen(msg)
        int  $0x80
     25:mov  $1, %ebx
     26:mov  $__NR_exit_group, %eax
        jmp  24b

        // The first page is mapped read-only for use as securely shared memory
     27:movd %mm6, %edi          // %edi = old_shared_mem
        mov  0x44(%edi), %ebx    // addr = secure_mem
        movd %ebx, %mm5          // %mm5 = secure_mem
        movd %mm2, %esi
        cmp  %esi, 4(%edi)
        jne  fatal_error
        mov  $__NR_mprotect, %eax
        mov  $4096, %ecx         // len  = 4096
        mov  $1, %edx            // prot = PROT_READ
        int  $0x80
        CHECK_SYSCALL_ZERO

        // The second page is used as scratch space by the trusted thread.
        // Make it writable.
        mov  $__NR_mprotect, %eax
        add  $4096, %ebx         // addr = secure_mem + 4096
        mov  $3, %edx            // prot = PROT_READ | PROT_WRITE
        int  $0x80
        CHECK_SYSCALL_ZERO

        // Call clone() to create new trusted thread().
        // clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|
        //       CLONE_SYSVSEM|CLONE_UNTRACED, stack, NULL, NULL, NULL)
        mov  4(%ebp), %eax       // threadFd (on child's stack)
        movd %eax, %mm0          // %mm0  = threadFd
        mov  $__NR_clone, %eax
        mov  $0x850F00, %ebx     // flags = VM|FS|FILES|SIGH|THR|SYSV|UTR
        mov  $1, %ecx            // stack = 1
        cmp  %esi, 4(%edi)
        jne  fatal_error
        int  $0x80
        test %eax, %eax
        js   fatal_error
        jz   1b                  // invoke trustedThreadFnc()

        // Set up thread local storage
        mov  $0x51, %eax         // seg_32bit, limit_in_pages, useable
        mov  %eax, -0x04(%ebp)
        mov  $0xFFFFF, %eax      // limit
        mov  %eax, -0x08(%ebp)
        movd %mm5, %eax
        add  $0x58, %eax
        mov  %eax, -0x0C(%ebp)   // base_addr = &secure_mem.TLS
        mov  %fs, %eax
        shr  $3, %eax
        mov  %eax, -0x10(%ebp)   // entry_number
        mov  $__NR_set_thread_area, %eax
        lea  -0x10(%ebp), %ebx
        int  $0x80
        CHECK_SYSCALL_ZERO

        // Copy the caller's signal mask
        movd %mm5, %edx
        mov  0x1038(%edi), %eax
        mov  %eax, 0x1038(%edx)
        mov  0x103C(%edi), %eax
        mov  %eax, 0x103C(%edx)

        // Done creating trusted thread. We can get ready to return to caller
        mov  0(%ebp), %esi       // %esi = threadFdPub
        add  $8, %ebp

        // Check the sequence number
        movd %mm2, %edx
        cmp  %edx, 4(%edi)
        jne  fatal_error

        // Nascent thread launches a helper that doesn't share any of our
        // resources, except for pages mapped as MAP_SHARED.
        // clone(SIGCHLD, stack=1)
        mov  $__NR_clone, %eax
        mov  $17, %ebx           // flags = SIGCHLD
        mov  $1, %ecx            // stack = 1
        int  $0x80
        test %eax, %eax
        js   fatal_error
        jne  28f

        // Use sendmsg() to send to the trusted process the file handles for
        // communicating with the new trusted thread. We also send the address
        // of the secure memory area (for sanity checks) and the thread id.
        cmp  %edx, 4(%edi)
        jne  fatal_error

        // 0x00 socketcall:
        //   0x00 socket         (cloneFdPub)
        //   0x04 msg            (%ecx + 0x0C)
        //   0x08 flags          ($0)
        // 0x0C msg:
        //   0x0C msg_name       ($0)
        //   0x10 msg_namelen    ($0)
        //   0x14 msg_iov        (%ecx + 0x34)
        //   0x18 msg_iovlen     ($1)
        //   0x1C msg_control    (%ecx + 0x3C)
        //   0x20 msg_controllen ($0x14)
        // 0x24 data:
        //   0x24 msg_flags/err  ($0)
        //   0x28 secure_mem     (%mm5)
        //   0x2C threadId       (%mm4)
        //   0x30 threadFdPub    (%esi)
        // 0x34 iov:
        //   0x34 iov_base       (%ecx + 0x24)
        //   0x38 iov_len        ($0x10)
        // 0x3C cmsg:
        //   0x3C cmsg_len       ($0x14)
        //   0x40 cmsg_level     ($1, SOL_SOCKET)
        //   0x44 cmsg_type      ($1, SCM_RIGHTS)
        //   0x48 threadFdPub    (%esi)
        //   0x4C threadFd       (%mm0)
        // 0x50
        movd %mm1, %ecx
        add  $(sendmsg_data-0b), %ecx
        xor  %eax, %eax
        mov  %eax, 0x08(%ecx)    // flags
        mov  %eax, 0x0C(%ecx)    // msg_name
        mov  %eax, 0x10(%ecx)    // msg_namelen
        mov  %eax, 0x24(%ecx)    // msg_flags
        inc  %eax
        mov  %eax, 0x18(%ecx)    // msg_iovlen
        mov  %eax, 0x40(%ecx)    // cmsg_level
        mov  %eax, 0x44(%ecx)    // cmsg_type
        movl $0x10, 0x38(%ecx)   // iov_len
        mov  $0x14, %eax
        mov  %eax, 0x20(%ecx)    // msg_controllen
        mov  %eax, 0x3C(%ecx)    // cmsg_len
        movd %mm3, %eax          // cloneFdPub
        mov  %eax, 0x00(%ecx)    // socket
        lea  0x0C(%ecx), %eax
        mov  %eax, 0x04(%ecx)    // msg
        add  $0x18, %eax
        mov  %eax, 0x34(%ecx)    // iov_base
        add  $0x10, %eax
        mov  %eax, 0x14(%ecx)    // msg_iov
        add  $8, %eax
        mov  %eax, 0x1C(%ecx)    // msg_control
        mov  %esi, 0x30(%ecx)    // threadFdPub
        mov  %esi, 0x48(%ecx)    // threadFdPub
        movd %mm5, %eax
        mov  %eax, 0x28(%ecx)    // secure_mem
        movd %mm4, %eax
        mov  %eax, 0x2C(%ecx)    // threadId
        movd %mm0, %eax
        mov  %eax, 0x4C(%ecx)    // threadFd
        mov  $16, %ebx           // sendmsg()
        mov  $__NR_socketcall, %eax
        int  $0x80
        xor  %ebx, %ebx
        jmp  26b                 // exit process (no error message)

        // Reap helper
     28:mov  %eax, %ebx
     29:lea  -4(%ebp), %ecx
        xor  %edx, %edx
        mov  $__NR_waitpid, %eax
        int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   29b
        mov  -4(%ebp), %eax
        test %eax, %eax
        jnz  25b                 // exit process (no error message)

        // Release privileges by entering seccomp mode.
        mov  $__NR_prctl, %eax
        mov  $22, %ebx           // PR_SET_SECCOMP
        mov  $1, %ecx
        int  $0x80
        CHECK_SYSCALL_ZERO

        // We can finally start using the stack. Signal handlers no longer pose
        // a threat to us.
        mov  %ebp, %esp

        // Back in the newly created sandboxed thread, wait for trusted process
        // to receive request. It is possible for an attacker to make us
        // continue even before the trusted process is done. This is OK. It'll
        // result in us putting stale values into the new thread's TLS. But
        // that data is considered untrusted anyway.
        push %eax
        mov  $1, %edx            // len       = 1
        mov  %esp, %ecx          // buf       = %esp
        mov  %esi, %ebx          // fd        = threadFdPub
     30:mov  $__NR_read, %eax
        int  $0x80
        cmp  $-4, %eax           // EINTR
        jz   30b
        cmp  %edx, %eax
        jne  fatal_error
        pop  %eax

        // Returning to the place where clone() had been called. We rely on
        // using sigreturn() for restoring our registers. The caller already
        // created a signal stack frame and patched the register values
        // with the ones that were in effect prior to calling sandbox_clone().
        mov  $__NR_sigreturn, %eax
        int  $0x80

        .pushsection ".rodata"
    100:.ascii "Sandbox violation detected, program aborted\n"
    101:.ascii "WARNING! This is an expensive system call\n"
    102:
        .popsection

    999:pop  %ebp
        pop  %ebx
        ret


        .bss
        // Reserve space for sendmsg() data.  This is used in a fork()'d
        // helper process, so in principle this could safely overlap and
        // overwrite other data, but it is such a small amount of memory
        // that it is not worth trying to do that.  The only requirement
        // is that this must be in a MAP_PRIVATE mapping so that an
        // untrusted thread cannot modify the forked subprocess's copy.
sendmsg_data:
        .space 0x50
