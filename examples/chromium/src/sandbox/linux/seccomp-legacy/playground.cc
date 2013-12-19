// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(markus): remove. for debugging, only
// #define STRACE_HACK

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOINTR(x) ({ int i__; while ((i__ = (x)) < 0 && errno == EINTR); i__;})

struct user_regs_struct_32 {
  int ebx, ecx, edx, esi, edi, ebp, eax;
  unsigned short ds, __ds, es, __es;
  unsigned short fs, __fs, gs, __gs;
  unsigned orig_eax, eip;
  unsigned short cs, __cs;
  int eflags, esp;
  unsigned short ss, __ss;
};

struct user_regs_struct_64 {
  unsigned long r15,r14,r13,r12,rbp,rbx,r11,r10;
  unsigned long r9,r8,rax,rcx,rdx,rsi,rdi,orig_rax;
  unsigned long rip,cs,eflags;
  unsigned long rsp,ss;
  unsigned long fs_base, gs_base;
  unsigned long ds,es,fs,gs;
};

typedef union user_regs {
  struct user_regs_struct_32 regs32;
  struct user_regs_struct_64 regs64;
} UserRegs;

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    puts("Executable is missing from command line");
    return 1;
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  union {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } hdr;
  if (NOINTR(read(fd, &hdr.e32, sizeof(hdr.e32))) != sizeof(hdr.e32) ||
      memcmp(hdr.e32.e_ident, ELFMAG, SELFMAG) ||
      (hdr.e32.e_ident[EI_CLASS] == ELFCLASS64 &&
       NOINTR(read(fd, &hdr.e32 + 1, sizeof(hdr.e64) - sizeof(hdr.e32)) !=
              sizeof(hdr.e64) - sizeof(hdr.e32))) ||
      (hdr.e32.e_ident[EI_CLASS] != ELFCLASS32 &&
       hdr.e32.e_ident[EI_CLASS] != ELFCLASS64)) {
    puts("Cannot identify binary");
  }
  unsigned long entry;
  if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32) {
    entry = hdr.e32.e_entry;
  } else {
    entry = hdr.e64.e_entry;
  }
#ifdef STRACE_HACK
  int strace_fds[2];
  pipe(strace_fds);
#endif
  int fds[2];
  pipe(fds);
  pid_t parent = getpid();
  pid_t pid = fork();
  if (pid > 0) {
    // puts("In parent");
    (void)NOINTR(close(fds[1]));
    if (NOINTR(read(fds[0], fds + 1, 1)) != 1) {
      puts("Failed to attach sandbox");
      return 1;
    }
    (void)NOINTR(close(fds[0]));
    // puts("Parent is being traced");
    char exe[80];
    snprintf(exe, sizeof(exe), "/proc/self/fd/%d", fd);
    putenv(hdr.e32.e_ident[EI_CLASS] == ELFCLASS32 ?
           (char *)"LD_PRELOAD=./preload32.so" :
           (char *)"LD_PRELOAD=./preload64.so");
#ifdef STRACE_HACK
    dup2(strace_fds[0], 39);
    (void)NOINTR(close(strace_fds[0]));
    (void)NOINTR(close(strace_fds[1]));
#endif
    execv(exe, argv + 1);
  } else if (pid == 0) {
    (void)NOINTR(close(fds[0]));
    (void)NOINTR(close(fd));
    int status;
    // puts("Attaching to parent");
    if (ptrace(PTRACE_ATTACH, parent, 0, 0) == -1) {
      puts("Failed to attach to parent");
      return 1;
    }
    // puts("Waiting for parent to become traced");
    waitpid(parent, &status, __WALL);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      puts("Parent exited");
      return 1;
    }
    // puts("Setting marker in parent");
    ptrace(PTRACE_POKEDATA, parent, (unsigned long)&main,
           (long)0xCCCCCCCCCCCCCCCCll);
    // puts("Allowing parent to continue");
    (void)NOINTR(write(fds[1], "", 1));
    (void)NOINTR(close(fds[1]));
    do {
      // puts("Continuing parent");
      ptrace(PTRACE_SYSCALL, parent, 0, 0);
      // puts("Waiting for exec() to complete");
      waitpid(parent, &status, __WALL);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        puts("Parent exited");
        return 1;
      }
    } while (ptrace(PTRACE_PEEKDATA, parent, (unsigned long)&main, NULL) ==
             (long)0xCCCCCCCCCCCCCCCCll);
    printf("Setting breakpoint at 0x%lx\n", entry);
    long old_inst = ptrace(PTRACE_PEEKDATA, parent, entry, NULL);
    int bp  = (old_inst & ~0xFF) | 0xCC;
    ptrace(PTRACE_POKEDATA, parent, entry, bp);
    int signo = 0;
    UserRegs regs = { { 0 } };
    for (;;) {
      // puts("Continuing");
      ptrace(PTRACE_CONT, parent, signo, signo);
      waitpid(parent, &status, __WALL);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        puts("Parent exited");
        return 1;
      }
      signo = WIFSTOPPED(status) ? WSTOPSIG(status) : 0;
      if (signo != SIGTRAP) {
        printf("signal=%d\n", signo);
        continue;
      }
      signo = 0;
      ptrace(PTRACE_GETREGS, parent, NULL, &regs);
      if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32) {
        // printf("Parent hit bp at 0x%lx\n", (long)regs.regs32.eip);
        if (regs.regs32.eip == entry+1) {
          regs.regs32.eip = entry;
          break;
        }
      } else {
        // printf("Parent hit bp at 0x%lx\n", (long)regs.regs64.rip);
        if (regs.regs64.rip == entry+1) {
          regs.regs64.rip = entry;
          break;
        }
      }
    }
    char buf[128];
    snprintf(buf, sizeof(buf),
             "cat /proc/%d/maps|"
             "sed -e 's/-[^ ]* r-xp .*preload[63][42].so//;t1;d;:1;q'",
             parent);
    FILE *fp = popen(buf, "r");
    if (fp == NULL ||
        fgets(buf, sizeof(buf), fp) == NULL) {
      puts("Failed to find preload.so in parent's address space");
      return 1;
    }
    fclose(fp);
    long preload = strtol(buf, NULL, 16);
    // printf("preload.so loaded at 0x%lx\n", preload);
    snprintf(buf, sizeof(buf),
             "objdump -t preload%d.so|sed -e 's/ g.* preload$//;t1;d;:1;q'",
             hdr.e32.e_ident[EI_CLASS] == ELFCLASS32 ? 32 : 64);
    fp = popen(buf, "r");
    if (fp == NULL ||
        fgets(buf, sizeof(buf), fp) == NULL) {
      puts("Cannot find preload() function in preload.so");
      return 1;
    }
    fclose(fp);
    preload += strtol(buf, NULL, 16);
    // printf("preload() is located at 0x%lx\n", preload);
    ptrace(PTRACE_POKEDATA, parent, entry, old_inst);
    if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32) {
      regs.regs32.eip = preload;
    } else {
      regs.regs64.rip = preload;
    }
    ptrace(PTRACE_SETREGS, parent, NULL, &regs);
    signo = 0;
    UserRegs bp_regs = { { 0 } };
    for (;;) {
      // puts("Continuing");
      ptrace(PTRACE_CONT, parent, signo, signo);
      waitpid(parent, &status, __WALL);
      ptrace(PTRACE_GETREGS, parent, 0, &bp_regs);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        puts("Parent exited");
        return 1;
      }
      signo = WIFSTOPPED(status) ? WSTOPSIG(status) : 0;
      if (signo != SIGTRAP) {
        if (signo != SIGCHLD) {
          printf("signal=%d, eip=0x%lx\n", signo,
                 hdr.e32.e_ident[EI_CLASS] == ELFCLASS32 ?
                 (unsigned long)bp_regs.regs32.eip : bp_regs.regs64.rip);
        }
        continue;
      }
      signo = 0;
      printf("Resetting program counter from 0x%lx to 0x%lx\n",
             hdr.e32.e_ident[EI_CLASS] == ELFCLASS32 ?
             (unsigned long)bp_regs.regs32.eip : bp_regs.regs64.rip,
             entry);
      if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32) {
        regs.regs32.eip     = entry;
        regs.regs32.fs      = bp_regs.regs32.fs;
        regs.regs32.__fs    = bp_regs.regs32.__fs;
        regs.regs32.gs      = bp_regs.regs32.gs;
        regs.regs32.__gs    = bp_regs.regs32.__gs;
      } else {
        regs.regs64.rip     = entry;
        regs.regs64.fs      = bp_regs.regs64.fs;
        regs.regs64.fs_base = bp_regs.regs64.fs_base;
        regs.regs64.gs      = bp_regs.regs64.gs;
        regs.regs64.gs_base = bp_regs.regs64.gs_base;
      }
#ifdef STRACE_HACK
      if (hdr.e32.e_ident[EI_CLASS] == ELFCLASS32) {
        regs.regs32.eip = bp_regs.regs32.eip;
      } else {
        regs.regs64.rip = bp_regs.regs64.rip;
      }
#endif
      ptrace(PTRACE_SETREGS, parent, NULL, &regs);
      break;
    }
    puts("Detaching from parent");
    ptrace(PTRACE_DETACH, parent, 0, 0);

#ifdef STRACE_HACK
    (void)NOINTR(close(strace_fds[0]));

  #if 0
    char strace_buf[80];
    sprintf(strace_buf, "strace -f -o /tmp/strace.log -p %d &", parent);
    system(strace_buf);
  #else
    FILE *f = fopen("/tmp/gdbwrapper", "w");
    fprintf(f, "#!/bin/bash\nexec gdb %s %d", argv[1], parent);
    fchmod(fileno(f), 0755);
    fclose(f);
    system("xterm /tmp/gdbwrapper &");
  #endif

    sleep(1);
    write(strace_fds[1], &entry, sizeof(entry));
    (void)NOINTR(close(strace_fds[1]));
#endif
  } else {
    return 1;
  }
  puts("Sandbox launcher is done");
  return 0;
}
