// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox__clone(int flags, char* stack, int* pid, void* arg4,
                            void* arg5, void *wrapper_sp) {
  long long tm;
  Debug::syscall(&tm, __NR_clone, "Executing handler");
  struct {
    struct RequestHeader header;
    Clone     clone_req;
  } __attribute__((packed)) request;
  request.clone_req.flags      = flags;
  request.clone_req.stack      = stack;
  request.clone_req.pid        = pid;
  request.clone_req.arg4       = arg4;
  request.clone_req.arg5       = arg5;

  // TODO(markus): Passing stack == 0 currently does not do the same thing
  // that the kernel would do without the sandbox. This is just going to
  // cause a crash. We should detect this case, and replace the stack pointer
  // with the correct value, instead.
  // This is complicated by the fact that we will temporarily be executing
  // both threads from the same stack. Some synchronization will be necessary.
  // Fortunately, this complication also explains why hardly anybody ever
  // does this.
  // See trusted_thread.cc for more information.
  long rc;
  if (stack == 0) {
    rc = -EINVAL;
  } else {
    // In order to unblock the signal mask in the newly created thread and
    // after entering Seccomp mode, we have to call sigreturn(). But that
    // requires access to a proper stack frame describing a valid signal.
    // We trigger a signal now and make sure the stack frame ends up on the
    // new stack. Our segv() handler (in sandbox.cc) does that for us.
    // See trusted_thread.cc for more details on how threads get created.
    //
    // In general we rely on the kernel for generating the signal stack
    // frame, as the exact binary format has been extended several times over
    // the course of the kernel's development. Fortunately, the kernel
    // developers treat the initial part of the stack frame as a stable part
    // of the ABI. So, we can rely on fixed, well-defined offsets for accessing
    // register values and for accessing the signal mask.
    #if defined(__x86_64__)
    // Red zone compensation. The instrumented system call will remove 128
    // bytes from the thread's stack prior to returning to the original
    // call site.
    stack                   -= 128;
    request.clone_req.stack  = stack;
    void *dummy;
    asm volatile("mov %%rsp, %%rcx\n"
                 "mov %3, %%rsp\n"
                 "int $0\n"
                 "mov %%rcx, %%rsp\n"
                 : "=a"(request.clone_req.stack), "=&c"(dummy)
                 : "a"(__NR_clone + 0xF000), "m"(request.clone_req.stack)
                 : "memory");
    #elif defined(__i386__)
    void *dummy;
    asm volatile("mov %%esp, %%ecx\n"
                 "mov %3, %%esp\n"
                 "int $0\n"
                 "mov %%ecx, %%esp\n"
                 : "=a"(request.clone_req.stack), "=&c"(dummy)
                 : "a"(__NR_clone + 0xF000), "m"(request.clone_req.stack)
                 : "memory");
    #else
    #error Unsupported target platform
    #endif

    // We have created a signal frame that contains the correct values
    // of FP registers and segment registers, but we need to update it
    // with:
    //  * The values of general purpose registers, as saved by
    //    syscallEntryPoint.
    //  * The return address, which is also saved by syscallEntryPoint.
    //    Note that we do not return through defaultSystemCallHandler()
    //    and the syscallEntryPoint code in the new thread.  We jump
    //    out of these, straight back to where syscallEntryPoint was
    //    originally called.
    //  * The new stack address for the new thread.
    //  * The return value from the syscall (0 in the new thread).
    struct CloneStackFrame *regs = (struct CloneStackFrame *) wrapper_sp;
    #if defined(__x86_64__)
    struct ucontext *uc = (struct ucontext *) request.clone_req.stack;
    uc->uc_mcontext.gregs[REG_R8] = (long) regs->r8;
    uc->uc_mcontext.gregs[REG_R9] = (long) regs->r9;
    uc->uc_mcontext.gregs[REG_R10] = (long) regs->r10;
    uc->uc_mcontext.gregs[REG_R11] = (long) regs->r11;
    uc->uc_mcontext.gregs[REG_R12] = (long) regs->r12;
    uc->uc_mcontext.gregs[REG_R13] = (long) regs->r13;
    uc->uc_mcontext.gregs[REG_R14] = (long) regs->r14;
    uc->uc_mcontext.gregs[REG_R15] = (long) regs->r15;
    uc->uc_mcontext.gregs[REG_RDI] = (long) regs->rdi;
    uc->uc_mcontext.gregs[REG_RSI] = (long) regs->rsi;
    uc->uc_mcontext.gregs[REG_RBP] = (long) regs->rbp;
    uc->uc_mcontext.gregs[REG_RBX] = (long) regs->rbx;
    uc->uc_mcontext.gregs[REG_RDX] = (long) regs->rdx;
    uc->uc_mcontext.gregs[REG_RCX] = (long) regs->rcx;
    uc->uc_mcontext.gregs[REG_RAX] = 0; // Result of clone()
    uc->uc_mcontext.gregs[REG_RIP] = (long) regs->ret;
    uc->uc_mcontext.gregs[REG_RSP] = (long) stack;
    #elif defined(__i386__)
    struct sigcontext *sc = (struct sigcontext *) request.clone_req.stack;
    sc->edi = (long) regs->edi;
    sc->esi = (long) regs->esi;
    sc->ebp = (long) regs->ebp;
    sc->ebx = (long) regs->ebx;
    sc->edx = (long) regs->edx;
    sc->ecx = (long) regs->ecx;
    sc->eax = 0; // Result of clone()
    sc->eip = (long) regs->ret;
    sc->esp = (long) stack;
    #else
    #error Unsupported target platform
    #endif

    rc = forwardSyscall(__NR_clone, &request.header, sizeof(request));
  }
  Debug::elapsed(tm, __NR_clone);
  return rc;
}

bool Sandbox::process_clone(const SecureMem::SyscallRequestInfo* info) {
  // Read request
  Clone clone_req;
  SysCalls sys;
  if (read(sys, info->trustedProcessFd, &clone_req, sizeof(clone_req)) !=
      sizeof(clone_req)) {
    die("Failed to read parameters for clone() [process]");
  }

  // TODO(markus): add policy restricting parameters for clone
  if ((clone_req.flags & ~CLONE_DETACHED) != (CLONE_VM|CLONE_FS|CLONE_FILES|
      CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|
      CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID)) {
    SecureMem::abandonSystemCall(*info, -EPERM);
    return false;
  } else {
    SecureMem::Args* newMem  = getNewSecureMem();
    if (!newMem) {
      SecureMem::abandonSystemCall(*info, -ENOMEM);
      return false;
    } else {
      newMem->sequence       = 0;
      newMem->shmId          = -1;

      SecureMem::sendSystemCall(*info, SecureMem::SEND_UNLOCKED,
                                clone_req.flags, clone_req.stack,
                                clone_req.pid, clone_req.arg4, clone_req.arg5,
                                (void*)NULL, newMem);
      return true;
    }
  }
}

} // namespace
