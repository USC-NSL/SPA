// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/prctl.h>

#include "debug.h"
#include "sandbox_impl.h"
#include "system_call_table.h"


// This file implements TLS initialisation system calls so that ld.so
// (the glibc dynamic linker) can start up inside the sandbox.
//
// This is not straightforward because the set_thread_area() and
// arch_prctl() syscalls modify a thread's state, so we cannot simply
// forward the syscall to the trusted thread, because that would
// modify the wrong thread's state.
//
// Instead, we use the trick of using clone() to create a new thread
// that picks up where the original thread left off, but with modified
// thread state.  Furthermore, clone() provides a built-in way of
// setting up TLS.  Hence this requires no special support in the
// trusted code.
//
// There is a potential gotcha:  By replacing the thread, we are
// changing thread state, including:
//  * the thread ID
//  * the alternate signal stack, settable via sigaltstack()
//    (although the sandbox does not allow sigaltstack() anyway)
//  * child_tidptr, settable via set_tid_address()
// In practice, TLS initialisation is done only once, early on.  The
// dynamic linker does not query the thread ID before setting up TLS.


namespace playground {

#if defined(__i386__)
// clone() does not support allocating a new segment index, so we
// preallocate one before the sandbox is enabled.
static int g_preallocated_segment_index;
#endif

// The sandbox currently requires that we pass CLONE_CHILD_CLEARTID etc.,
// although we do not care about the value the kernel fills out, so we
// use this location and ignore the contents.
static int g_dummy_tid;

struct CloneRequest {
  struct Sandbox::RequestHeader clone_header;
  Sandbox::Clone clone_req;
  struct Sandbox::RequestHeader exit_header;
} __attribute__((packed));

extern void __attribute__((noreturn))
WriteAndExit(int fd, const void *buf, size_t count)
  asm("playground$writeAndExit");

static void SignalCallback(int sig, siginfo_t *info, void *uc1) {
  struct ucontext *uc = (struct ucontext *) uc1;

  #if defined(__x86_64__)
  long arg = uc->uc_mcontext.gregs[REG_RCX];
  // Skip over the "int $0" instruction.
  uc->uc_mcontext.gregs[REG_RIP] += 2;
  char *return_frame = (char *) uc;
  #elif defined(__i386__)
  long arg = uc->uc_mcontext.gregs[REG_ECX];
  // Skip over the "int $0" instruction.
  uc->uc_mcontext.gregs[REG_EIP] += 2;
  // We need to convert to the old signal frame format, because the
  // sandbox only allows sigreturn(), not rt_sigreturn().
  struct sigcontext sc;
  memset(&sc, 0, sizeof(sc));
  sc.gs = uc->uc_mcontext.gregs[REG_GS];
  sc.fs = uc->uc_mcontext.gregs[REG_FS];
  sc.es = uc->uc_mcontext.gregs[REG_ES];
  sc.ds = uc->uc_mcontext.gregs[REG_DS];
  sc.edi = uc->uc_mcontext.gregs[REG_EDI];
  sc.esi = uc->uc_mcontext.gregs[REG_ESI];
  sc.ebp = uc->uc_mcontext.gregs[REG_EBP];
  sc.esp = uc->uc_mcontext.gregs[REG_ESP];
  sc.ebx = uc->uc_mcontext.gregs[REG_EBX];
  sc.edx = uc->uc_mcontext.gregs[REG_EDX];
  sc.ecx = uc->uc_mcontext.gregs[REG_ECX];
  sc.eax = uc->uc_mcontext.gregs[REG_EAX];
  sc.trapno = uc->uc_mcontext.gregs[REG_TRAPNO];
  sc.err = uc->uc_mcontext.gregs[REG_ERR];
  sc.eip = uc->uc_mcontext.gregs[REG_EIP];
  sc.cs = uc->uc_mcontext.gregs[REG_CS];
  sc.eflags = uc->uc_mcontext.gregs[REG_EFL];
  sc.esp_at_signal = uc->uc_mcontext.gregs[REG_UESP];
  sc.ss = uc->uc_mcontext.gregs[REG_SS];
  sc.oldmask = uc->uc_mcontext.oldmask;
  sc.cr2 = uc->uc_mcontext.cr2;
  char *return_frame = (char *) &sc;
  #else
  #error Unsupported target platform
  #endif

  CloneRequest *request = (CloneRequest *) arg;
  request->clone_req.stack = return_frame;
  WriteAndExit(Sandbox::processFdPub(), request, sizeof(*request));
}

// This is like a CallWithEscapingContinuation() function, where the
// escaping continuation is a signal frame.
static void CallWithSignalFrame(void (*func)(int sig, siginfo_t *info,
                                             void *ucontext),
                                void *arg) {
  asm volatile("int $0\n"
               : : "a"(__NR_clone + 0xF001), "d"(func), "c"(arg));
}

static void SetTls(void *tls_info) {
  CloneRequest request;
  request.clone_header.sysnum = __NR_clone;
  request.clone_header.cookie = Sandbox::cookie();
  // request.clone_req.stack is filled out later.
  request.clone_req.flags = CLONE_VM|CLONE_FS|CLONE_FILES|
    CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|
    CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID;
  request.clone_req.pid = &g_dummy_tid;
  #if defined(__x86_64__)
  request.clone_req.arg4 = &g_dummy_tid; // ctid argument
  request.clone_req.arg5 = tls_info; // tls argument
  #elif defined(__i386__)
  request.clone_req.arg5 = &g_dummy_tid; // ctid argument
  request.clone_req.arg4 = tls_info; // tls argument
  #else
  #error Unsupported target platform
  #endif
  request.exit_header.sysnum = __NR_exit;
  request.exit_header.cookie = Sandbox::cookie();
  CallWithSignalFrame(SignalCallback, &request);
}

#if defined(__x86_64__)
static long sandbox_arch_prctl(int code, unsigned long addr) {
  long long tm;
  long rc;
  Debug::syscall(&tm, __NR_arch_prctl, "Executing handler");
  if (code == ARCH_SET_FS) {
    SetTls((void *) addr);
    rc = 0;
  } else {
    rc = -ENOSYS;
  }
  Debug::elapsed(tm, __NR_arch_prctl);
  return rc;
}
#endif

#if defined(__i386__)
static long sandbox_set_thread_area(struct user_desc *tls_desc) {
  long long tm;
  Debug::syscall(&tm, __NR_set_thread_area, "Executing handler");
  // clone() does not support allocating a new segment index: it does
  // not accept entry_number == -1, so we have to use one we have
  // allocated earlier.
  if (tls_desc->entry_number == (unsigned) -1) {
    tls_desc->entry_number = g_preallocated_segment_index;
    g_preallocated_segment_index = -1;
  }
  SetTls(tls_desc);
  Debug::elapsed(tm, __NR_set_thread_area);
  return 0;
}

static void PreallocateSegmentIndex() {
  struct user_desc tls_desc;
  tls_desc.entry_number = -1; // Allocate new segment selector
  tls_desc.base_addr = 0;
  tls_desc.limit = 0xfffff;
  tls_desc.seg_32bit = 1;
  tls_desc.contents = 0;
  tls_desc.read_exec_only = 0;
  tls_desc.limit_in_pages = 1;
  tls_desc.seg_not_present = 0;
  tls_desc.useable = 1;
  if (syscall(__NR_set_thread_area, &tls_desc) != 0) {
    Sandbox::die("set_thread_area() failed");
  }
  g_preallocated_segment_index = tls_desc.entry_number;
}
#endif

void AddTlsSetupSyscall() {
  SyscallTable::initializeSyscallTable();
  SyscallTable::unprotectSyscallTable();

#if defined(__x86_64__)
  SyscallTable::syscallTable[__NR_arch_prctl].handler =
      reinterpret_cast<void (*)()>(sandbox_arch_prctl);
#endif
#if defined(__i386__)
  PreallocateSegmentIndex();
  SyscallTable::syscallTable[__NR_set_thread_area].handler =
      reinterpret_cast<void (*)()>(sandbox_set_thread_area);
#endif

  SyscallTable::protectSyscallTable();
}

}
