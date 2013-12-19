// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_IMPL_H__
#define SANDBOX_IMPL_H__

#include <asm/ldt.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/prctl.h>
#include <linux/unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NOINTR_SYS(x)                                                         \
  ({ typeof(x) i__; while ((i__ = (x)) < 0 && sys.my_errno == EINTR); i__;})

#if defined(__NR_ipc)
# ifndef SHMAT
#  define SHMAT       21
# endif
# ifndef SHMDT
#  define SHMDT       22
# endif
# ifndef SHMGET
#  define SHMGET      23
# endif
# ifndef SHMCTL
#  define SHMCTL      24
# endif
#endif

#include "system_call_table.h"

#include <map>
#include <vector>
#include "sandbox.h"
#include "securemem.h"
#include "tls.h"

#ifndef INTERNAL
#define INTERNAL __attribute__((visibility("internal")))
#endif

namespace playground {

class Sandbox {
  // TODO(markus): restrict access to our private file handles
 public:
  enum { kMaxThreads = 100 };


  // There are a lot of reasons why the Seccomp sandbox might not be available.
  // This could be because the kernel does not support Seccomp mode, or it
  // could be because we fail to successfully rewrite all system call entry
  // points.
  // "proc" should be a file descriptor for "/proc", or -1 if not provided by
  // the caller.
  static int supportsSeccompSandbox(int proc)
                                         asm("SupportsSeccompSandbox");

  // The sandbox needs to be able to access "/proc/self/maps". If this file
  // is not accessible when "startSandbox()" gets called, the caller can
  // provide an already opened file descriptor by calling "setProcFd()".
  // The sandbox becomes the newer owner of this file descriptor and will
  // eventually close it. After calling setProcFd(), the caller MUST eventually
  // call startSandbox(), or resources could be leaked.
  static void setProcFd(int proc)        asm("SeccompSandboxSetProcFd");

  // This is the main public entry point. It finds all system calls that
  // need rewriting, sets up the resources needed by the sandbox, and
  // enters Seccomp mode.
  static void startSandbox()             asm("StartSeccompSandbox");

  // TODO(mseaborn): Consider re-instating this declaration.
  // private:

  struct RequestHeader {
    unsigned int sysnum;
    long long cookie;
  } __attribute__((packed));

  // This forwards a system call to the trusted process.
  static long forwardSyscall(int sysnum, struct RequestHeader* request,
                             int size);

  // Clone() is special as it has a wrapper in syscall_entrypoint.c. The wrapper
  // adds one extra argument (the pointer to the saved registers) and then
  // calls playground$sandbox__clone().
  // arg4 and arg5 are given non-specific names because their meanings
  // are reversed between i386 and x86-64.
  static long sandbox_clone(int flags, char* stack, int* pid, void* arg4,
                          void* arg5) asm("playground$sandbox_clone") INTERNAL;
  static long sandbox__clone(int flags, char* stack, int* pid, void* arg4,
                             void* arg5, void* wrapper_sp)
                                     asm("playground$sandbox__clone") INTERNAL;

  // Entry points for sandboxed code that is attempting to make system calls
  static long sandbox_access(const char*, int);
  static long sandbox_exit(int status);
  static long sandbox_getpid();
  #if defined(__NR_getsockopt)
  static long sandbox_getsockopt(int, int, int, void*, socklen_t*);
  #endif
  static long sandbox_gettid();
  static long sandbox_ioctl(int d, int req, void* arg);
  #if defined(__NR_ipc)
  static long sandbox_ipc(unsigned, int, int, int, void*, long);
  #endif
  static long sandbox_lstat(const char* path, void* buf);
  #if defined(__NR_lstat64)
  static long sandbox_lstat64(const char *path, void* b);
  #endif
  static long sandbox_madvise(void*, size_t, int);
  static void *sandbox_mmap(void* start, size_t length, int prot, int flags,
                            int fd, off_t offset);
  static long sandbox_mprotect(const void*, size_t, int);
  static long sandbox_munmap(void* start, size_t length);
  static long sandbox_open(const char*, int, mode_t);
  static long sandbox_prctl(int, unsigned long, unsigned long, unsigned long,
                            unsigned long);
  #if defined(__NR_recvfrom)
  static ssize_t sandbox_recvfrom(int, void*, size_t, int, void*, socklen_t*);
  static ssize_t sandbox_recvmsg(int, struct msghdr*, int);
  #endif
  #if defined(__NR_rt_sigaction)
  static long sandbox_rt_sigaction(int, const void*, void*, size_t);
  #endif
  #if defined(__NR_rt_sigprocmask)
  static long sandbox_rt_sigprocmask(int how, const void*, void*, size_t);
  #endif
  #if defined(__NR_sendmsg)
  static size_t sandbox_sendmsg(int, const struct msghdr*, int);
  static ssize_t sandbox_sendto(int, const void*, size_t, int, const void*,
                                socklen_t);
  #endif
  #if defined(__NR_shmat)
  static void* sandbox_shmat(int, const void*, int);
  static long sandbox_shmctl(int, int, void*);
  static long sandbox_shmdt(const void*);
  static long sandbox_shmget(int, size_t, int);
  #endif
  #if defined(__NR_setsockopt)
  static long sandbox_setsockopt(int, int, int, const void*, socklen_t);
  #endif
  #if defined(__NR_sigaction)
  static long sandbox_sigaction(int, const void*, void*);
  #endif
  #if defined(__NR_signal)
  static void* sandbox_signal(int, const void*);
  #endif
  #if defined(__NR_sigprocmask)
  static long sandbox_sigprocmask(int how, const void*, void*);
  #endif
  #if defined(__NR_socketcall)
  static long sandbox_socketcall(int call, void* args);
  #endif
  static long sandbox_stat(const char* path, void* buf);
  #if defined(__NR_stat64)
  static long sandbox_stat64(const char *path, void* b);
  #endif

  // Functions for system calls that need to be handled in the trusted process
  static bool process_access(const SyscallRequestInfo* info);
  static bool process_clone(const SyscallRequestInfo* info);
  static bool process_exit(const SyscallRequestInfo* info);
  #if defined(__NR_getsockopt)
  static bool process_getsockopt(const SyscallRequestInfo* info);
  #endif
  static bool process_ioctl(const SyscallRequestInfo* info);
  #if defined(__NR_ipc)
  static bool process_ipc(const SyscallRequestInfo* info);
  #endif
  static bool process_madvise(const SyscallRequestInfo* info);
  static bool process_mmap(const SyscallRequestInfo* info);
  static bool process_mprotect(const SyscallRequestInfo* info);
  static bool process_munmap(const SyscallRequestInfo* info);
  static bool process_open(const SyscallRequestInfo* info);
  static bool process_prctl(const SyscallRequestInfo* info);
  #if defined(__NR_recvfrom)
  static bool process_recvfrom(const SyscallRequestInfo* info);
  static bool process_recvmsg(const SyscallRequestInfo* info);
  static bool process_sendmsg(const SyscallRequestInfo* info);
  static bool process_sendto(const SyscallRequestInfo* info);
  static bool process_setsockopt(const SyscallRequestInfo* info);
  #endif
  #if defined(__NR_shmat)
  static bool process_shmat(const SyscallRequestInfo* info);
  static bool process_shmctl(const SyscallRequestInfo* info);
  static bool process_shmdt(const SyscallRequestInfo* info);
  static bool process_shmget(const SyscallRequestInfo* info);
  #endif
  static bool process_sigaction(const SyscallRequestInfo* info);
  #if defined(__NR_socketcall)
  static bool process_socketcall(const SyscallRequestInfo* info);
  #endif
  static bool process_stat(const SyscallRequestInfo* info);

  friend class Debug;
  friend class Library;
  friend class Maps;
  friend class Mutex;
  friend class SecureMem;
  friend class TLS;

  // Define our own inline system calls. These calls will not be rewritten
  // to point to the sandboxed wrapper functions. They thus allow us to
  // make actual system calls (e.g. in the sandbox initialization code, and
  // in the trusted process)
  class SysCalls {
   public:
    #define SYS_CPLUSPLUS
    #define SYS_ERRNO     my_errno
    #define SYS_INLINE    inline
    #define SYS_PREFIX    -1
    #undef  SYS_LINUX_SYSCALL_SUPPORT_H
    #include "linux_syscall_support.h"
    SysCalls() : my_errno(0) { }
    int my_errno;
  };
  #ifdef __NR_mmap2
    #define      MMAP      mmap2
    #define __NR_MMAP __NR_mmap2
  #else
    #define      MMAP      mmap
    #define __NR_MMAP __NR_mmap
  #endif

  // Print an error message and terminate the program. Used for fatal errors.
  static void die(const char *msg = 0) __attribute__((noreturn)) {
    SysCalls sys;
    if (msg) {
      sys.write(2, msg, strlen(msg));
      sys.write(2, "\n", 1);
    }
    for (;;) {
      sys.exit_group(1);
      sys._exit(1);
    }
  }

  // Wrapper around "read()" that can deal with partial and interrupted reads
  // and that does not modify the global errno variable.
  static ssize_t read(SysCalls& sys, int fd, void* buf, size_t len) {
    if (static_cast<ssize_t>(len) < 0) {
      sys.my_errno = EINVAL;
      return -1;
    }
    size_t offset = 0;
    while (offset < len) {
      ssize_t partial =
          NOINTR_SYS(sys.read(fd, reinterpret_cast<char*>(buf) + offset,
                              len - offset));
      if (partial < 0) {
        return partial;
      } else if (!partial) {
        break;
      }
      offset += partial;
    }
    return offset;
  }

  // Wrapper around "write()" that can deal with interrupted writes and that
  // does not modify the global errno variable.
  static ssize_t write(SysCalls& sys, int fd, const void* buf, size_t len){
    return NOINTR_SYS(sys.write(fd, buf, len));
  }

  // Sends a file handle to another process.
  // N.B. trusted_thread.cc has an assembly version of this function that
  //      is safe to use without a call stack. If the wire-format is changed,
  ///     make sure to update the assembly code.
  static bool sendFd(int transport, int fd0, int fd1, const void* buf,
                     size_t len);

  // If getFd() fails, it will set the first valid fd slot (e.g. fd0) to
  // -errno.
  static bool getFd(int transport, int* fd0, int* fd1, void* buf,
                    size_t* len);

  // Data structures used to forward system calls to the trusted process.
  struct Accept {
    int        sockfd;
    void*      addr;
    socklen_t* addrlen;
  } __attribute__((packed));

  struct Accept4 {
    int        sockfd;
    void*      addr;
    socklen_t* addrlen;
    int        flags;
  } __attribute__((packed));

  struct Access {
    size_t path_length;
    int    mode;
  } __attribute__((packed));

  struct Bind {
    int       sockfd;
    void*     addr;
    socklen_t addrlen;
  } __attribute__((packed));

  #if defined(__x86_64__)
  struct CloneStackFrame {
    void* r15;
    void* r14;
    void* r13;
    void* r12;
    void* r11;
    void* r10;
    void* r9;
    void* r8;
    void* rdi;
    void* rsi;
    void* rdx;
    void* rcx;
    void* rbx;
    void* deadbeef_marker;
    void* rbp;
    void* fake_ret;
    void* ret;
  } __attribute__((packed));
  #elif defined(__i386__)
  struct CloneStackFrame {
    void* edi;
    void* esi;
    void* edx;
    void* ecx;
    void* ebx;
    void* deadbeef_marker;
    void* ebp;
    void* fake_ret;
    void* ret;
  } __attribute__((packed));
  #else
  #error Unsupported target platform
  #endif

  struct Clone {
    int       flags;
    char*     stack;
    int*      pid;
    void*     arg4; // ctid on x86-64; tls on i386
    void*     arg5; // tls on x86-64; ctid on i386
  } __attribute__((packed));

  struct Connect {
    int       sockfd;
    void*     addr;
    socklen_t addrlen;
  } __attribute__((packed));

  struct GetSockName {
    int        sockfd;
    void*      name;
    socklen_t* namelen;
  } __attribute__((packed));

  struct GetPeerName {
    int        sockfd;
    void*      name;
    socklen_t* namelen;
  } __attribute__((packed));

  struct GetSockOpt {
    int        sockfd;
    int        level;
    int        optname;
    void*      optval;
    socklen_t* optlen;
  } __attribute__((packed));

  struct IOCtl {
    int  d;
    int  req;
    void *arg;
  } __attribute__((packed));

  #if defined(__NR_ipc)
  struct IPC {
    unsigned call;
    int      first;
    int      second;
    int      third;
    void*    ptr;
    long     fifth;
  } __attribute__((packed));
  #endif

  struct Listen {
    int sockfd;
    int backlog;
  } __attribute__((packed));

  struct MAdvise {
    const void*  start;
    size_t       len;
    int          advice;
  } __attribute__((packed));

  struct MMap {
    void*  start;
    size_t length;
    int    prot;
    int    flags;
    int    fd;
    off_t  offset;
  } __attribute__((packed));

  struct MProtect {
    const void*  addr;
    size_t       len;
    int          prot;
  };

  struct MUnmap {
    void*  start;
    size_t length;
  } __attribute__((packed));

  struct Open {
    size_t path_length;
    int    flags;
    mode_t mode;
  } __attribute__((packed));

  struct Prctl {
    int    option;
    unsigned long arg2;
    unsigned long arg3;
    unsigned long arg4;
    unsigned long arg5;
  } __attribute__((packed));

  struct Recv {
    int    sockfd;
    void*  buf;
    size_t len;
    int    flags;
  } __attribute__((packed));

  struct RecvFrom {
    int       sockfd;
    void*     buf;
    size_t    len;
    int       flags;
    void*     from;
    socklen_t *fromlen;
  } __attribute__((packed));

  struct RecvMsg {
    int                  sockfd;
    struct msghdr*       msg;
    int                  flags;
  } __attribute__((packed));

  struct Send {
    int         sockfd;
    const void* buf;
    size_t      len;
    int         flags;
  } __attribute__((packed));

  struct SendMsg {
    int                  sockfd;
    const struct msghdr* msg;
    int                  flags;
  } __attribute__((packed));

  struct SendTo {
    int         sockfd;
    const void* buf;
    size_t      len;
    int         flags;
    const void* to;
    socklen_t   tolen;
  } __attribute__((packed));

  struct SetSockOpt {
    int         sockfd;
    int         level;
    int         optname;
    const void* optval;
    socklen_t   optlen;
  } __attribute__((packed));

  #if defined(__NR_shmat)
  struct ShmAt {
    int         shmid;
    const void* shmaddr;
    int         shmflg;
 } __attribute__((packed));

  struct ShmCtl {
    int  shmid;
    int  cmd;
    void *buf;
  } __attribute__((packed));

  struct ShmDt {
    const void *shmaddr;
  } __attribute__((packed));

  struct ShmGet {
    int    key;
    size_t size;
    int    shmflg;
  } __attribute__((packed));
  #endif

  struct ShutDown {
    int sockfd;
    int how;
  } __attribute__((packed));

  struct SigAction {
    int                               signum;
    const SysCalls::kernel_sigaction* action;
    const SysCalls::kernel_sigaction* old_action;
    size_t                            sigsetsize;
  } __attribute__((packed));

  struct Socket {
    int domain;
    int type;
    int protocol;
  } __attribute__((packed));

  struct SocketPair {
    int  domain;
    int  type;
    int  protocol;
    int* pair;
  } __attribute__((packed));

  #if defined(__NR_socketcall)
  struct SocketCall {
    int    call;
    void*  arg_ptr;
    union {
      Socket      socket;
      Bind        bind;
      Connect     connect;
      Listen      listen;
      Accept      accept;
      GetSockName getsockname;
      GetPeerName getpeername;
      SocketPair  socketpair;
      Send        send;
      Recv        recv;
      SendTo      sendto;
      RecvFrom    recvfrom;
      ShutDown    shutdown;
      SetSockOpt  setsockopt;
      GetSockOpt  getsockopt;
      SendMsg     sendmsg;
      RecvMsg     recvmsg;
      Accept4     accept4;
    } args;
  } __attribute__((packed));
  #endif

  struct Stat {
    size_t path_length;
    void*  buf;
  } __attribute__((packed));

  // Thread local data available from each sandboxed thread.
  enum { TLS_COOKIE, TLS_TID, TLS_THREAD_FD };
  static long long cookie() { return TLS::getTLSValue<long long>(TLS_COOKIE); }
  static int tid()          { return TLS::getTLSValue<int>(TLS_TID); }
  static int threadFdPub()  { return TLS::getTLSValue<int>(TLS_THREAD_FD); }
  static int processFdPub() { return processFdPub_; }
  static kernel_sigset_t* signalMask() { return &getSecureMem()->signalMask; }

  // The SEGV handler knows how to handle RDTSC instructions
  static void setupSignalHandlers();
  static void (*segv())(int signo, SysCalls::siginfo *context, void *unused);

  // If no specific handler has been registered for a system call, call this
  // function which asks the trusted thread to perform the call. This is used
  // for system calls that are not restricted.
  static void* defaultSystemCallHandler(int syscallNum, void* arg0,
                                        void* arg1, void* arg2, void* arg3,
                                        void* arg4, void* arg5)
                           asm("playground$defaultSystemCallHandler") INTERNAL;

  // Return the current secure memory structure for this thread.
  static SecureMem::Args* getSecureMem();

  // Return a secure memory structure that can be used by a newly created
  // thread.
  static SecureMem::Args* getNewSecureMem();

  // This functions runs in the trusted process at startup and finds all the
  // memory mappings that existed when the sandbox was first enabled. Going
  // forward, all these mappings are off-limits for operations such as
  // mmap(), munmap(), and mprotect().
  static int   initializeProtectedMap(int fd);

  // Returns whether the given memory range is protected and may not be
  // modified by the sandboxed process.
  static bool  isRegionProtected(void* addr, size_t size);

  // Helper functions that allows the trusted process to get access to
  // "/proc/self/maps" in the sandbox.
  static void  snapshotMemoryMappings(int processFd, int proc_self_maps);

  // Main loop for the trusted process.
  static void  trustedProcess(int parentMapsFd, int processFdPub,
                              int sandboxFd, int cloneFd,
                              SecureMem::Args* secureArena)
                                                     __attribute__((noreturn));

  // Fork()s of the trusted process.
  static SecureMem::Args* createTrustedProcess(int processFdPub, int sandboxFd,
                                               int cloneFdPub, int cloneFd);

  // Creates the trusted thread for the initial thread, then enables
  // Seccomp mode.
  static void  createTrustedThread(SecureMem::Args* secureMem);

  static int   proc_;
  static int   proc_self_maps_;
  static enum SandboxStatus {
    STATUS_UNKNOWN, STATUS_UNSUPPORTED, STATUS_AVAILABLE, STATUS_ENABLED
  }            status_;
  static int   pid_;
  static int   processFdPub_;
  static int   cloneFdPub_ asm("playground$cloneFdPub") INTERNAL;

  #ifdef __i386__
  struct SocketCallArgInfo;
  static const struct SocketCallArgInfo socketCallArgInfo[];
  #endif

  // We always have to intercept SIGSEGV. If the application wants to set its
  // own SEGV handler, we forward to it whenever necessary.
  static SysCalls::kernel_sigaction sa_segv_ asm("playground$sa_segv")INTERNAL;

  // Available in trusted process, only
  typedef std::map<void *, long>       ProtectedMap;
  static ProtectedMap                  protectedMap_;
  static std::vector<SecureMem::Args*> secureMemPool_;
};

void CreateReferenceTrustedThread(SecureMem::Args* secureMem);

// If this struct is extended to contain parameters that are read by
// the trusted thread, we will have to mprotect() it to be read-only when
// starting the sandbox.  However, currently it is read only by the
// trusted process, and the sandboxed process cannot change the values
// that the fork()'d trusted process sees.
struct SandboxPolicy {
  bool allow_file_namespace;  // Allow filename-based system calls.
  bool unrestricted_sysv_mem; // Do not place restrictions on SysV shared mem
};

extern struct SandboxPolicy g_policy;

typedef void (*CreateTrustedThreadFunc)(SecureMem::Args* secureMem);
extern CreateTrustedThreadFunc g_create_trusted_thread;

} // namespace

using playground::Sandbox;

#endif // SANDBOX_IMPL_H__
