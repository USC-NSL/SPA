// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox_impl.h"

#include "library.h"
#include "syscall_entrypoint.h"
#include "system_call_table.h"

namespace playground {

// Global variables
int                                 Sandbox::proc_self_maps_ = -1;
enum Sandbox::SandboxStatus         Sandbox::status_ = STATUS_UNKNOWN;
int                                 Sandbox::pid_;
int                                 Sandbox::processFdPub_;
int                                 Sandbox::cloneFdPub_
  // This is necessary to locate the symbol from assembly code on
  // x86-64 (with %rip-relative addressing) in order for this to work
  // in relocatable code (a .so or a PIE).  On i386 this is not
  // necessary but it does not hurt.
  __attribute__((visibility("internal")));
Sandbox::SysCalls::kernel_sigaction Sandbox::sa_segv_;
Sandbox::ProtectedMap               Sandbox::protectedMap_;
std::vector<SecureMem::Args*>       Sandbox::secureMemPool_;
CreateTrustedThreadFunc             g_create_trusted_thread =
  Sandbox::createTrustedThread;

bool Sandbox::sendFd(int transport, int fd0, int fd1, const void* buf,
                     size_t len) {
  int fds[2], count                     = 0;
  if (fd0 >= 0) { fds[count++]          = fd0; }
  if (fd1 >= 0) { fds[count++]          = fd1; }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec  iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg    = { 0 };
  int dummy                             = 0;
  iov[0].iov_base                       = &dummy;
  iov[0].iov_len                        = sizeof(dummy);
  if (buf && len > 0) {
    iov[1].iov_base                     = const_cast<void *>(buf);
    iov[1].iov_len                      = len;
  }
  msg.msg_iov                           = iov;
  msg.msg_iovlen                        = (buf && len > 0) ? 2 : 1;
  msg.msg_control                       = cmsg_buf;
  msg.msg_controllen                    = CMSG_LEN(count*sizeof(int));
  struct cmsghdr *cmsg                  = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level                      = SOL_SOCKET;
  cmsg->cmsg_type                       = SCM_RIGHTS;
  cmsg->cmsg_len                        = CMSG_LEN(count*sizeof(int));
  memcpy(CMSG_DATA(cmsg), fds, count*sizeof(int));
  SysCalls sys;
  return NOINTR_SYS(sys.sendmsg(transport, &msg, 0)) ==
      (ssize_t)(sizeof(dummy) + ((buf && len > 0) ? len : 0));
}

bool Sandbox::getFd(int transport, int* fd0, int* fd1, void* buf, size_t*len) {
  int count                            = 0;
  int *err                             = NULL;
  if (fd0) {
    count++;
    err                                = fd0;
    *fd0                               = -1;
  }
  if (fd1) {
    if (!count++) {
      err                              = fd1;
    }
    *fd1                               = -1;
  }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg   = { 0 };
  iov[0].iov_base                      = err;
  iov[0].iov_len                       = sizeof(int);
  if (buf && len && *len > 0) {
    iov[1].iov_base                    = buf;
    iov[1].iov_len                     = *len;
  }
  msg.msg_iov                          = iov;
  msg.msg_iovlen                       = (buf && len && *len > 0) ? 2 : 1;
  msg.msg_control                      = cmsg_buf;
  msg.msg_controllen                   = CMSG_LEN(count*sizeof(int));
  SysCalls sys;
  ssize_t bytes = NOINTR_SYS(sys.recvmsg(transport, &msg, 0));
  if (len) {
    *len                               = bytes > (int)sizeof(int) ?
                                           bytes - sizeof(int) : 0;
  }
  if (bytes != (ssize_t)(sizeof(int) + ((buf && len && *len > 0) ? *len : 0))){
    *err                               = bytes >= 0 ? 0 : -EBADF;
    return false;
  }
  if (*err) {
    // "err" is the first four bytes of the payload. If these are non-zero,
    // the sender on the other side of the socketpair sent us an errno value.
    // We don't expect to get any file handles in this case.
    return false;
  }
  struct cmsghdr *cmsg               = CMSG_FIRSTHDR(&msg);
  if ((msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) ||
      !cmsg                                    ||
      cmsg->cmsg_level != SOL_SOCKET           ||
      cmsg->cmsg_type  != SCM_RIGHTS           ||
      cmsg->cmsg_len   != CMSG_LEN(count*sizeof(int))) {
    *err                             = -EBADF;
    return false;
  }
  if (fd1) { *fd1 = ((int *)CMSG_DATA(cmsg))[--count]; }
  if (fd0) { *fd0 = ((int *)CMSG_DATA(cmsg))[--count]; }
  return true;
}

void segvSignalHandler(int signo, Sandbox::SysCalls::siginfo *context,
                       void *unused)
  asm("playground$segvSignalHandler") INTERNAL;

void Sandbox::setupSignalHandlers() {
  // Set SIGCHLD to SIG_DFL so that waitpid() can work
  SysCalls sys;
  struct SysCalls::kernel_sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler_ = SIG_DFL;
  sys.sigaction(SIGCHLD, &sa, NULL);

  // Set up SEGV handler for dealing with RDTSC instructions, system calls
  // that have been rewritten to use INT0, for sigprocmask() emulation, for
  // the creation of threads, and for user-provided SEGV handlers.
  sa.sa_sigaction_ = segvSignalHandler;
  sa.sa_flags      = SA_SIGINFO | SA_NODEFER;
  sys.sigaction(SIGSEGV, &sa, &sa_segv_);

  // Unblock SIGSEGV and SIGCHLD
  SysCalls::kernel_sigset_t mask;
  memset(&mask, 0x00, sizeof(mask));
  mask.sig[0] |= (1 << (SIGSEGV - 1)) | (1 << (SIGCHLD - 1));
  sys.sigprocmask(SIG_UNBLOCK, &mask, 0);
}

long Sandbox::forwardSyscall(int sysnum, struct RequestHeader* request,
                             int size) {
  SysCalls sys;
  long rc;
  request->sysnum = sysnum;
  request->cookie = cookie();
  if (write(sys, processFdPub(), request, size) != size) {
    die("Failed to send forwarded request");
  }
  if (read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to receive forwarded result");
  }
  return rc;
}

SecureMem::Args* Sandbox::getSecureMem() {
  // Check trusted_thread.cc for the magic offset that gets us from the TLS
  // to the beginning of the secure memory area.
  SecureMem::Args* ret;
#if defined(__x86_64__)
  asm volatile(
    "movq %%gs:-0xE0, %0\n"
    : "=q"(ret));
#elif defined(__i386__)
  asm volatile(
    "movl %%fs:-0x58, %0\n"
    : "=r"(ret));
#else
#error Unsupported target platform
#endif
  return ret;
}

void Sandbox::snapshotMemoryMappings(int processFd, int proc_self_maps) {
  SysCalls sys;
  if (sys.lseek(proc_self_maps, 0, SEEK_SET) ||
      !sendFd(processFd, proc_self_maps, -1, NULL, 0)) {
 failure:
    die("Cannot access /proc/self/maps");
  }
  int dummy;
  if (read(sys, processFd, &dummy, sizeof(dummy)) != sizeof(dummy)) {
    goto failure;
  }
}

int Sandbox::supportsSeccompSandbox(int proc) {
  if (status_ != STATUS_UNKNOWN) {
    return status_ != STATUS_UNSUPPORTED;
  }
  int fds[2];
  SysCalls sys;
  if (sys.pipe(fds)) {
    status_ = STATUS_UNSUPPORTED;
    return 0;
  }
  pid_t pid;
  switch ((pid = sys.fork())) {
    case -1:
      status_ = STATUS_UNSUPPORTED;
      return 0;
    case 0: {
      int devnull = sys.open("/dev/null", O_RDWR, 0);
      if (devnull >= 0) {
        sys.dup2(devnull, 0);
        sys.dup2(devnull, 1);
        sys.dup2(devnull, 2);
        sys.close(devnull);
      }
      if (proc >= 0) {
        setProcFd(proc);
      }
      startSandbox();
      write(sys, fds[1], "", 1);

      // Try to tell the trusted thread to shut down the entire process in an
      // orderly fashion
      defaultSystemCallHandler(__NR_exit_group, 0, 0, 0, 0, 0, 0);

      // If that did not work (e.g. because the kernel does not know about the
      // exit_group() system call), make a direct _exit() system call instead.
      // This system call is unrestricted in seccomp mode, so it will always
      // succeed. Normally, we don't like it, because unlike exit_group() it
      // does not terminate any other thread. But since we know that
      // exit_group() exists in all kernels which support kernel-level threads,
      // this is OK we only get here for old kernels where _exit() is OK.
      sys._exit(0);
    }
    default:
      (void)NOINTR_SYS(sys.close(fds[1]));
      char ch;
      if (read(sys, fds[0], &ch, 1) != 1) {
        status_ = STATUS_UNSUPPORTED;
      } else {
        status_ = STATUS_AVAILABLE;
      }
      int rc;
      (void)NOINTR_SYS(sys.waitpid(pid, &rc, 0));
      (void)NOINTR_SYS(sys.close(fds[0]));
      return status_ != STATUS_UNSUPPORTED;
  }
}

void Sandbox::setProcFd(int proc) {
  if (proc >= 0) {
    SysCalls sys;
    proc_self_maps_ = sys.openat(proc, "self/maps", O_RDONLY, 0);
    if (NOINTR_SYS(sys.close(proc))) {
      die("Failed to close file descriptor pointing to /proc");
    }
  }
}

void Sandbox::startSandbox() {
  if (status_ == STATUS_UNSUPPORTED) {
    die("The seccomp sandbox is not supported on this computer");
  } else if (status_ == STATUS_ENABLED) {
    return;
  }

  SysCalls sys;
  if (proc_self_maps_ < 0) {
    proc_self_maps_        = sys.open("/proc/self/maps", O_RDONLY, 0);
    if (proc_self_maps_ < 0) {
      die("Cannot access \"/proc/self/maps\"");
    }
  }

  // The pid is unchanged for the entire program, so we can retrieve it once
  // and store it in a global variable.
  pid_                     = sys.getpid();

  // Block all signals, except for the RDTSC handler
  setupSignalHandlers();

  // Set up the system call policy
  SyscallTable::initializeSyscallTable();

  // Get socketpairs for talking to the trusted process
  int pair[4];
  if (sys.socketpair(AF_UNIX, SOCK_STREAM, 0, pair) ||
      sys.socketpair(AF_UNIX, SOCK_STREAM, 0, pair+2)) {
    die("Failed to create trusted thread");
  }
  processFdPub_            = pair[0];
  cloneFdPub_              = pair[2];
  SecureMemArgs* secureMem = createTrustedProcess(pair[0], pair[1],
                                                  pair[2], pair[3]);

  // We find all libraries that have system calls and redirect the system
  // calls to the sandbox. If we miss any system calls, the application will be
  // terminated by the kernel's seccomp code. So, from a security point of
  // view, if this code fails to identify system calls, we are still behaving
  // correctly.
  {
    Maps maps(proc_self_maps_);
    const char *libs[]     = { "ld", "libc", "librt", "libpthread", NULL };

    // Intercept system calls in the VDSO segment (if any). This has to happen
    // before intercepting system calls in any of the other libraries, as
    // the main kernel entry point might be inside of the VDSO and we need to
    // determine its address before we can compare it to jumps from inside
    // other libraries.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      if (library->isVDSO() && library->parseElf()) {
        library->makeWritable(true);
        library->patchSystemCalls();
        library->makeWritable(false);
        break;
      }
    }

    // Intercept system calls in libraries that are known to have them.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      const char* mapping = iter.name().c_str();

      // Find the actual base name of the mapped library by skipping past any
      // SPC and forward-slashes. We don't want to accidentally find matches,
      // because the directory name included part of our well-known lib names.
      //
      // Typically, prior to pruning, entries would look something like this:
      // 08:01 2289011 /lib/libc-2.7.so
      for (const char *delim = " /"; *delim; ++delim) {
        const char* skip = strrchr(mapping, *delim);
        if (skip) {
          mapping = skip + 1;
        }
      }

      for (const char **ptr = libs; *ptr; ptr++) {
        const char *name = strstr(mapping, *ptr);
        if (name == mapping) {
          char ch = name[strlen(*ptr)];
          if (ch < 'A' || (ch > 'Z' && ch < 'a') || ch > 'z') {
            if (library->parseElf()) {
              library->makeWritable(true);
              library->patchSystemCalls();
              library->makeWritable(false);
              break;
            }
          }
        }
      }
    }
  }

  // Take a snapshot of the current memory mappings. These mappings will be
  // off-limits to all future mmap(), munmap(), mremap(), and mprotect() calls.
  // This also provides a synchronization point that ensures the trusted
  // process has finished initialization.
  snapshotMemoryMappings(processFdPub_, proc_self_maps_);
  (void)NOINTR_SYS(sys.close(proc_self_maps_));
  proc_self_maps_ = -1;

  // Creating the trusted thread enables sandboxing
  g_create_trusted_thread(secureMem);

  // Force direct system calls to jump to our entry point.
  struct {
    // Instantiate another copy of linux_syscall_support.h. This time, we
    // define SYS_SYSCALL_ENTRYPOINT. This gives us access to a
    // get_syscall_entrypoint() function that we can use to install a pointer
    // to our system call entrypoint handler.
    // Any user of linux_syscall_support.h who wants to make sure that the
    // sandbox properly redirects its system calls would define the same
    // macro.
    #undef  SYS_ERRNO
    #define SYS_INLINE             inline
    #define SYS_PREFIX             -1
    #define SYS_SYSCALL_ENTRYPOINT "playground$syscallEntryPoint"
    #undef  SYS_LINUX_SYSCALL_SUPPORT_H
    #include "linux_syscall_support.h"
  } entrypoint;
  *entrypoint.get_syscall_entrypoint() = syscallEntryPointNoFrame;

  // We can no longer check for sandboxing support at this point, but we also
  // know for a fact that it is available (as we just turned it on). So update
  // the status to reflect this information.
  status_ = STATUS_ENABLED;
}

} // namespace
