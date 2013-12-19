// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "syscall_entrypoint.h"

#include "debug.h"
#include "sandbox_impl.h"
#include "system_call_table.h"

namespace playground {

// TODO(markus): change this into a function that returns the address of the assembly code. If that isn't possible for sandbox_clone, then move that function into a *.S file
asm(
    ".pushsection .text, \"ax\", @progbits\n"

    // This is the special wrapper for the clone() system call. The code
    // relies on the stack layout of the system call entrypoint (c.f. below). It
    // passes the stack pointer as an additional argument to sandbox__clone(),
    // so that upon starting the child, register values can be restored and
    // the child can start executing at the correct IP, instead of trying to
    // run in the trusted thread.
    "playground$sandbox_clone:"
    ".internal playground$sandbox_clone\n"
    ".globl playground$sandbox_clone\n"
    ".type playground$sandbox_clone, @function\n"
    #if defined(__x86_64__)
    // Skip the 8 byte return address into the system call entrypoint. The
    // following bytes are the saved register values that we need to restore
    // upon return from clone() in the new thread.
    "lea 8(%rsp), %r9\n"
    "jmp playground$sandbox__clone\n"
    #elif defined(__i386__)
    // As i386 passes function arguments on the stack, we need to skip a few
    // more values before we can get to the saved registers.
    "mov 28(%esp), %eax\n"
    "mov %eax, 24(%esp)\n"
    "jmp playground$sandbox__clone\n"
    #else
    #error Unsupported target platform
    #endif
    ".size playground$sandbox_clone, .-playground$sandbox_clone\n"


    // This is the entrypoint which is called by the untrusted code, trying to
    // make a system call.
    "playground$syscallEntryPointNoFrame:"
    ".internal playground$syscallEntryPointNoFrame\n"
    ".globl playground$syscallEntryPointNoFrame\n"
    ".type playground$syscallEntryPointNoFrame, @function\n"
    #if defined(__x86_64__)
    "mov  0(%rsp), %r11\n"         // add fake return address by duplicating
    "push %r11\n"                  // real return address
    /* fall through */
    #elif defined(__i386__)
    "push %eax\n"                  // add fake return address, which in this
    "mov  4(%esp), %eax\n"         // case is identical to the real return
    "xchg %eax, 0(%esp)\n"         // address
    /* fall through */
    #else
    #error Unsupported target platform
    #endif
    ".size playground$syscallEntryPointNoFrame, "
        ".-playground$syscallEntryPointNoFrame\n"

    "playground$syscallEntryPointWithFrame:"
    ".internal playground$syscallEntryPointWithFrame\n"
    ".globl playground$syscallEntryPointWithFrame\n"
    ".type playground$syscallEntryPointWithFrame, @function\n"
    #if defined(__x86_64__)
    // Check for rt_sigreturn(). It needs to be handled specially.
    "cmp  $15, %rax\n"             // NR_rt_sigreturn
    "jnz  1f\n"
    "add  $0x90, %rsp\n"           // pop return addresses and red zone
  "0:syscall\n"                    // rt_sigreturn() is unrestricted
    "mov  $66, %edi\n"             // rt_sigreturn() should never return
    "mov  $231, %eax\n"            // NR_exit_group
    "jmp  0b\n"

    // Save all registers
  "1:push %rbp\n"
    "movq  $0xDEADBEEFDEADBEEF, %rbp\n" // marker used by breakpad to remove
    "push %rbp\n"                  // seccomp-sandbox's stack frame from dumps
    "mov  %rsp, %rbp\n"
    "push %rbx\n"
    "push %rcx\n"
    "push %rdx\n"
    "push %rsi\n"
    "push %rdi\n"
    "push %r8\n"
    "push %r9\n"
    "push %r10\n"
    "push %r11\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"

    // TODO(markus): On x86-32 we have to explicitly align the stack. Do we
    //               also have to do this on x86-64? Empirical evidence
    //               suggests, we are OK -- but we might have to revisit this
    //               decision.

    // Check range of system call
    "mov playground$maxSyscall@GOTPCREL(%rip), %rcx\n"
    "cmp 0(%rcx), %eax\n"
    "ja  3f\n"

    // Retrieve function call from system call table (c.f.system_call_table.cc)
    // We have three different types of entries; zero for denied system calls,
    // that should be handled by the defaultSystemCallHandler(); minus one
    // for unrestricted system calls that need to be forwarded to the trusted
    // thread; and function pointers to specific handler functions.
    "mov %rax, %rcx\n"
    "shl $4, %rcx\n"
    "mov playground$syscallTable@GOTPCREL(%rip), %r11\n"
    "mov 0(%r11), %r11\n"
    "add %r11, %rcx\n"
    "mov 0(%rcx), %rcx\n"

    // Jump to function if non-null and not UNRESTRICTED_SYSCALL, otherwise
    // jump to fallback handler.
    "cmp $1, %rcx\n"
    "jbe 3f\n"
    "xchg %r10, %rcx\n"            // Syscall to userspace calling conventions
    "call *%r10\n"
  "2:"

    // Restore CPU registers, except for %rax which was set by the system call.
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %r11\n"
    "pop %r10\n"
    "pop %r9\n"
    "pop %r8\n"
    "pop %rdi\n"
    "pop %rsi\n"
    "pop %rdx\n"
    "pop %rcx\n"
    "pop %rbx\n"
    "pop %rbp\n"                   // 0xDEADBEEF marker
    "pop %rbp\n"

    // Remove fake return address. This is added in the patching code in
    // library.cc and it makes stack traces a little cleaner.
    "add $8, %rsp\n"

    // Return to caller
    "ret\n"

  "3:"
    // If we end up calling a specific handler, we don't need to know the
    // system call number. However, in the generic case, we do. Shift
    // registers so that the system call number becomes visible as the
    // first function argument.
    "push %r9\n"
    "mov  %r8, %r9\n"
    "mov  %r10, %r8\n"
    "mov  %rdx, %rcx\n"
    "mov  %rsi, %rdx\n"
    "mov  %rdi, %rsi\n"
    "mov  %rax, %rdi\n"

    // Call default handler.
    "call playground$defaultSystemCallHandler\n"
    "pop  %r9\n"
    "jmp 2b\n"
    #elif defined(__i386__)
    "cmp  $119, %eax\n"            // NR_sigreturn
    "jnz  1f\n"
    "add  $0x8, %esp\n"            // pop return address
  "0:int  $0x80\n"                 // sigreturn() is unrestricted
    "mov  $66, %ebx\n"             // sigreturn() should never return
    "mov  %ebx, %eax\n"            // NR_exit
    "jmp  0b\n"
  "1:cmp  $173, %eax\n"            // NR_rt_sigreturn
    "jnz  4f\n"

    // Convert rt_sigframe into sigframe, allowing us to call sigreturn().
    // This is possible since the first part of signal stack frames have
    // stayed very stable since the earliest kernel versions. While never
    // officially documented, lots of user space applications rely on this
    // part of the ABI, and kernel developers have been careful to maintain
    // backwards compatibility.
    // In general, the rt_sigframe includes a lot of extra information that
    // the signal handler can look at. Most notably, this means a complete
    // siginfo record.
    // Fortunately though, the kernel doesn't look at any of this extra data
    // when returning from a signal handler. So, we can safely convert an
    // rt_sigframe to a legacy sigframe, discarding the extra data in the
    // process. Interestingly, the legacy signal frame is actually larger than
    // the rt signal frame, as it includes a lot more padding.
    "sub  $0x1C4, %esp\n"          // a legacy signal stack is much larger
    "mov  0x1CC(%esp), %eax\n"     // push signal number
    "push %eax\n"
    "lea  0x270(%esp), %esi\n"     // copy siginfo register values
    "lea  0x4(%esp), %edi\n"       //     into new location
    "mov  $0x16, %ecx\n"
    "cld\n"
    "rep movsl\n"
    "mov  0x2C8(%esp), %ebx\n"     // copy first half of signal mask
    "mov  %ebx, 0x54(%esp)\n"
    "call 2f\n"
  "2:pop  %esi\n"
    "add  $(3f-2b), %esi\n"
    "push %esi\n"                  // push restorer function
    "lea  0x2D4(%esp), %edi\n"     // patch up retcode magic numbers
    "movb $2, %cl\n"
    "rep movsl\n"
    "ret\n"                        // return to restorer function
  "3:pop  %eax\n"                  // remove dummy argument (signo)
    "mov  $119, %eax\n"            // NR_sigaction
    "int  $0x80\n"


    // Preserve all registers
  "4:push %ebp\n"
    "push $0xDEADBEEF\n"           // marker used by breakpad
    "push %ebx\n"
    "push %ecx\n"
    "push %edx\n"
    "push %esi\n"
    "push %edi\n"

    // Align stack pointer, so that called functions can push SSE registers
    // onto stack. This apparently is a requirement of the x86-32 ABI.
    "mov  %esp, %ebp\n"
    "and  $-16, %esp\n"
    "sub $4, %esp\n"
    "push %ebp\n"                  // push old un-aligned stack pointer
    "lea  0x14(%ebp), %ebp\n"      // frame pointer points to 0xDEADBEEF
    "push %eax\n"
    "mov  4(%ebp), %eax\n"         // push original value of %ebp
    "xchg %eax, 0(%esp)\n"

    // Convert from syscall calling conventions to C calling conventions
    "push %edi\n"
    "push %esi\n"
    "push %edx\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"

    // Check range of system call
    "call 5f\n"
  "5:pop  %edx\n"
    "add $(_GLOBAL_OFFSET_TABLE_+(.-5b)), %edx\n"
    "mov playground$maxSyscall@GOT(%edx), %edx\n"
    "cmp 0(%edx), %eax\n"
    "ja  14f\n"

    // We often have long sequences of calls to gettimeofday(). This is
    // needlessly expensive. Coalesce them into a single call.
    //
    // We keep track of state in TLS storage that we can access through
    // the %fs segment register. See trusted_thread.cc for the exact
    // memory layout.
    //
    // TODO(markus): maybe, we should proactively call gettimeofday() and
    //               clock_gettime(), whenever we talk to the trusted thread?
    //               or maybe, if we have recently seen requests to compute
    //               the time. There might be a repeated pattern of those.
    "cmp  $78, %eax\n"             // __NR_gettimeofday
    "jnz  10f\n"
    "cmp  %eax, %fs:0x102C-0x58\n" // last system call
    "jnz  7f\n"

    // This system call and the last system call prior to this one both are
    // calls to gettimeofday(). Try to avoid making the new call and just
    // return the same result as in the previous call.
    // Just in case the caller is spinning on the result from gettimeofday(),
    // every so often, call the actual system call.
    "decl %fs:0x1030-0x58\n"       // countdown calls to gettimofday()
    "jz   7f\n"

    // Atomically read the 64bit word representing last-known timestamp and
    // return it to the caller. On x86-32 this is a little more complicated and
    // requires the use of the cmpxchg8b instruction.
    "mov  %ebx, %eax\n"
    "mov  %ecx, %edx\n"
    "call 6f\n"
  "6:pop %ebp\n"
    "add $(100f-6b), %ebp\n"
    "lock; cmpxchg8b 0(%ebp)\n"
    "mov  %eax, 0(%ebx)\n"
    "mov  %edx, 4(%ebx)\n"
    "xor  %eax, %eax\n"
    "add  $28, %esp\n"
    "jmp  13f\n"

    // This is a call to gettimeofday(), but we don't have a valid cached
    // result, yet.
  "7:mov  %eax, %fs:0x102C-0x58\n" // remember syscall number
    "movl $500, %fs:0x1030-0x58\n" // make system call, each 500 invocations
    "call playground$defaultSystemCallHandler@PLT\n"

    // Returned from gettimeofday(). Remember return value, in case the
    // application calls us again right away.
    // Again, this has to happen atomically and requires cmpxchg8b.
    "mov 4(%ebx), %ecx\n"
    "mov 0(%ebx), %ebx\n"
    "call 8f\n"
  "8:pop %ebp\n"
    "add $(100f-8b), %ebp\n"
    "mov 0(%ebp), %eax\n"
    "mov 4(%ebp), %edx\n"
  "9:lock; cmpxchg8b 0(%ebp)\n"
    "jnz 9b\n"
    "xor %eax, %eax\n"
    "jmp 15f\n"

    // Remember the number of the last system call made. We deliberately do
    // not remember calls to gettid(), as we have often seen long sequences
    // of calls to just gettimeofday() and gettid(). In that situation, we
    // would still like to coalesce the gettimeofday() calls.
 "10:cmp $224, %eax\n"             // __NR_gettid
    "jz  11f\n"
    "mov  %eax, %fs:0x102C-0x58\n" // remember syscall number

    // Retrieve function call from system call table (c.f.system_call_table.cc)
    // We have three different types of entries; zero for denied system calls,
    // that should be handled by the defaultSystemCallHandler(); minus one
    // for unrestricted system calls that need to be forwarded to the trusted
    // thread; and function pointers to specific handler functions.
 "11:shl  $3, %eax\n"
    "call 12f\n"
 "12:pop  %ebx\n"
    "add  $(_GLOBAL_OFFSET_TABLE_+(.-12b)), %ebx\n"
    "mov  playground$syscallTable@GOT(%ebx), %ebx\n"
    "add  0(%ebx), %eax\n"
    "mov  0(%eax), %eax\n"

    // Jump to function if non-null and not UNRESTRICTED_SYSCALL, otherwise
    // jump to fallback handler.
    "cmp  $1, %eax\n"
    "jbe  14f\n"
    "add  $4, %esp\n"
    "call *%eax\n"
    "add  $24, %esp\n"

    // Restore CPU registers, except for %eax which was set by the system call.
 "13:pop  %esp\n"
    "pop  %edi\n"
    "pop  %esi\n"
    "pop  %edx\n"
    "pop  %ecx\n"
    "pop  %ebx\n"
    "pop  %ebp\n"                  // 0xDEADBEEF marker
    "pop  %ebp\n"

    // Remove fake return address. This is added in the patching code in
    // library.cc and it makes stack traces a little cleaner.
    "add  $4, %esp\n"

    // Return to caller
    "ret\n"

    // Call default handler.
 "14:call playground$defaultSystemCallHandler@PLT\n"
 "15:add  $28, %esp\n"
    "jmp  13b\n"

    ".pushsection \".bss\"\n"
    ".balign 8\n"
"100:.byte 0, 0, 0, 0, 0, 0, 0, 0\n"
    ".popsection\n"

    #else
    #error Unsupported target platform
    #endif
    ".size playground$syscallEntryPointWithFrame,"
        ".-playground$syscallEntryPointWithFrame\n"
    ".popsection\n"
);


void* Sandbox::defaultSystemCallHandler(int syscallNum, void* arg0, void* arg1,
                                        void* arg2, void* arg3, void* arg4,
                                        void* arg5) {
  // TODO(markus): The following comment is currently not true, we do intercept these system calls. Try to fix that.

  // We try to avoid intercepting read(), and write(), as these system calls
  // are not restricted in Seccomp mode. But depending on the exact
  // instruction sequence in libc, we might not be able to reliably
  // filter out these system calls at the time when we instrument the code.
  SysCalls  sys;
  long      rc;
  long long tm;
  switch (syscallNum) {
    case __NR_read:
      Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
      rc             = sys.read((long)arg0, arg1, (size_t)arg2);
      break;
    case __NR_write:
      Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
      rc             = sys.write((long)arg0, arg1, (size_t)arg2);
      break;
    default:
      if (Debug::isEnabled()) {
        // In debug mode, prevent stderr from being closed
        if (syscallNum == __NR_close && arg0 == (void *)2)
          return 0;
      }

      if ((unsigned)syscallNum <= SyscallTable::maxSyscall &&
         SyscallTable::syscallTable[syscallNum].handler==UNRESTRICTED_SYSCALL){
        Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
     perform_unrestricted:
        struct {
          int          sysnum;
          void*        unrestricted_req[6];
        } __attribute__((packed)) request = {
          syscallNum, { arg0, arg1, arg2, arg3, arg4, arg5 } };

        int   thread = threadFdPub();
        void* rc;
        if (write(sys, thread, &request, sizeof(request)) != sizeof(request) ||
            read(sys, thread, &rc, sizeof(rc)) != sizeof(rc)) {
          die("Failed to forward unrestricted system call");
        }
        Debug::elapsed(tm, syscallNum);
        return rc;
      } else if (Debug::isEnabled()) {
        Debug::syscall(&tm, syscallNum,
                       "In production mode, this call would be disallowed");
        goto perform_unrestricted;
      } else {
        return (void *)-ENOSYS;
      }
  }
  if (rc < 0) {
    rc               = -sys.my_errno;
  }
  Debug::elapsed(tm, syscallNum);
  return (void *)rc;
}

} // namespace
