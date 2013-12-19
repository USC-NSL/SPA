// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "library.h"
#include "sandbox.h"
#include "test_runner.h"


extern "C" int my_getpid(void);
extern char my_getpid_end[];

void patch_range(char *start, char *end) {
  int maps_fd;
  CHECK_SUCCEEDS((maps_fd = open("/proc/self/maps", O_RDONLY, 0)) >= 0);
  playground::Maps maps(maps_fd);
  playground::Library library;
  library.setLibraryInfo(&maps);
  char *extra_space = NULL;
  int extra_size = 0;
  char *page_start = (char *) ((uintptr_t) start & ~(getpagesize() - 1));
  CHECK_SUCCEEDS(mprotect(page_start, end - page_start,
                          PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
  library.patchSystemCallsInRange(start, end, &extra_space, &extra_size);
  CHECK_SUCCEEDS(close(maps_fd) == 0);
}

TEST(test_patching_syscall) {
  int pid = getpid();
  CHECK(my_getpid() == pid);
  char *func = (char *) my_getpid;
  char *func_end = my_getpid_end;
  patch_range(func, func_end);
#if defined(__x86_64__)
  CHECK(func[0] == '\xe9'); // e9 XX XX XX XX   jmp X
  CHECK(func[5] == '\x90'); // 90               nop
  CHECK(func[6] == '\x90'); // 90               nop
  CHECK(func[7] == '\xc3'); // c3               ret (unmodified)
#elif defined(__i386__)
  CHECK(func[0] == '\x68'); // 68 XX XX XX XX   push $X
  CHECK(func[5] == '\xc3'); // c3               ret
  CHECK(func[6] == '\x90'); // 90               nop
  CHECK(func[7] == '\xc3'); // c3               ret (unmodified)
#else
# error Unsupported target platform
#endif
  StartSeccompSandbox();
  CHECK(my_getpid() == pid);
}

#if defined(__x86_64__)

// These test cases test patching calls to the vsyscall page, which is
// present on x86-64 only.

// The timer tests below could fail on a heavily loaded machine, but
// we make a generous allowance for this.  They could also fail if the
// clock is changed while the test is running.
const int kMaxTime = 30; // Time in seconds

extern "C" int my_vgettimeofday(struct timeval *tv, struct timezone *tz);
extern char my_vgettimeofday_end[];

extern "C" int my_vtime(time_t *time);
extern char my_vtime_end[];

extern "C" int my_vgetcpu(unsigned *cpu, unsigned *node, void *tcache);
extern char my_vgetcpu_end[];

void check_patching_vsyscall(char *func, char *func_end) {
  patch_range(func, func_end);
  CHECK(func[0] == '\x48');  // 48 83 ec 08      sub $8, %rsp (unmodified)
  CHECK(func[1] == '\x83');
  CHECK(func[2] == '\xec');
  CHECK(func[3] == '\x08');
  CHECK(func[4] == '\xe9');  // e9 XX XX XX XX   jmp X
  CHECK(func[9] == '\x90');  // 90               nop
  CHECK(func[10] == '\x90'); // 90               nop
  CHECK(func[11] == '\x90'); // 90               nop
  CHECK(func[12] == '\x90'); // 90               nop
  CHECK(func[13] == '\x48'); // 48 83 c4 08      add $8, %rsp (unmodified)
  CHECK(func[14] == '\x83');
  CHECK(func[15] == '\xc4');
  CHECK(func[16] == '\x08');
  CHECK(func[17] == '\xc3'); // c3               ret (unmodified)
}

TEST(test_patching_vsyscall_gettimeofday) {
  struct timeval time1;
  struct timeval time2;
  CHECK_SUCCEEDS(gettimeofday(&time1, NULL) == 0);
  CHECK(my_vgettimeofday(&time2, NULL) == 0);
  CHECK(time1.tv_sec <= time2.tv_sec && time2.tv_sec < time1.tv_sec + kMaxTime);

  check_patching_vsyscall((char *) my_vgettimeofday, my_vgettimeofday_end);

  StartSeccompSandbox();
  CHECK(my_vgettimeofday(&time2, NULL) == 0);
  CHECK(time1.tv_sec <= time2.tv_sec && time2.tv_sec < time1.tv_sec + kMaxTime);
}

TEST(test_patching_vsyscall_time) {
  time_t time1;
  time_t time2;
  CHECK_SUCCEEDS((time1 = time(NULL)) != -1);
  time2 = time(NULL);
  CHECK(time1 <= time2 && time2 < time1 + kMaxTime);

  check_patching_vsyscall((char *) my_vtime, my_vtime_end);

  StartSeccompSandbox();
  time2 = time(NULL);
  CHECK(time1 <= time2 && time2 < time1 + kMaxTime);
}

TEST(test_patching_vsyscall_getcpu) {
  CHECK(my_vgetcpu(NULL, NULL, NULL) == 0);

  check_patching_vsyscall((char *) my_vgetcpu, my_vgetcpu_end);

  StartSeccompSandbox();
  // glibc's sched_getcpu() could still succeed if it goes via the
  // vdso and just reads memory, but my_vgetcpu() is always redirected
  // through the sandbox's handler and is rejected.
  CHECK(my_vgetcpu(NULL, NULL, NULL) == -ENOSYS);
}

#endif
