// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_runner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sandbox_impl.h"


struct testcase {
  const char *test_name;
  void (*test_func)();
};

static const int kMaxTests = 1000;
static struct testcase g_tests[kMaxTests];
static int g_tests_count = 0;

static int g_intended_status_fd = -1;


void add_test_case(const char *test_name, void (*test_func)()) {
  CHECK(g_tests_count < kMaxTests - 1);
  g_tests[g_tests_count].test_name = test_name;
  g_tests[g_tests_count].test_func = test_func;
  g_tests_count++;
}

// Reverse the test list so that tests are run in the order in which
// they appear in the source.  We do this because the constructor
// functions that we use to add the tests get run in reverse order.
static void reverse_tests() {
  int i = 0;
  int j = g_tests_count - 1;
  while (i < j) {
    struct testcase temp = g_tests[i];
    g_tests[i] = g_tests[j];
    g_tests[j] = temp;
    i++;
    j--;
  }
}

// Declares the wait() status that the test subprocess intends to exit with.
void intend_exit_status(int val, bool is_signal) {
  if (is_signal) {
    val = W_EXITCODE(0, val);
  } else {
    val = W_EXITCODE(val, 0);
  }
  if (g_intended_status_fd != -1) {
    CHECK_SUCCEEDS(write(g_intended_status_fd, &val, sizeof(val)) ==
                   sizeof(val));
  } else {
    // This prints in cases where we run one test without forking
    printf("Intending to exit with status %i...\n", val);
  }
}

static int run_test_forked(struct testcase *test) {
  printf("** %s\n", test->test_name);
  int pipe_fds[2];
  CHECK_SUCCEEDS(pipe(pipe_fds) == 0);
  pid_t pid;
  CHECK_SUCCEEDS((pid = fork()) >= 0);
  if (pid == 0) {
    CHECK_SUCCEEDS(close(pipe_fds[0]) == 0);
    g_intended_status_fd = pipe_fds[1];

    test->test_func();
    intend_exit_status(0, false);
    _exit(0);
  }
  CHECK_SUCCEEDS(close(pipe_fds[1]) == 0);

  int intended_status;
  int got = read(pipe_fds[0], &intended_status, sizeof(intended_status));
  bool got_intended_status = got == sizeof(intended_status);
  if (!got_intended_status) {
    printf("Test runner: Did not receive intended status\n");
  }

  int status;
  CHECK_SUCCEEDS(waitpid(pid, &status, 0) == pid);
  if (!got_intended_status) {
    printf("Test returned exit status %i\n", status);
    return 1;
  }
  else if ((status & ~WCOREFLAG) != intended_status) {
    printf("Test failed with exit status %i, expected %i\n",
           status, intended_status);
    return 1;
  }
  else {
    return 0;
  }
}

static int run_test_by_name(const char *name) {
  struct testcase *test;
  for (test = g_tests; test->test_name != NULL; test++) {
    if (strcmp(name, test->test_name) == 0) {
      printf("Running test %s...\n", name);
      test->test_func();
      printf("OK\n");
      return 0;
    }
  }
  fprintf(stderr, "Test '%s' not found\n", name);
  return 1;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  // Set a modest limit on the stack size, so that if we trigger
  // infinite recursion during the tests we do not trash the system by
  // triggering the OOM killer.
  struct rlimit limit;
  CHECK_SUCCEEDS(getrlimit(RLIMIT_STACK, &limit) == 0);
  limit.rlim_cur = MIN(limit.rlim_cur, 1024 * 1024);
  CHECK_SUCCEEDS(setrlimit(RLIMIT_STACK, &limit) == 0);

  if (getenv("SECCOMP_SANDBOX_REFERENCE_IMPL")) {
    // Insecure version, for development purposes.
    playground::g_create_trusted_thread =
      playground::CreateReferenceTrustedThread;
  }

  reverse_tests();

  if (argc == 2) {
    // Run one test without forking, to aid debugging.
    return run_test_by_name(argv[1]);
  }
  else if (argc > 2) {
    // TODO: run multiple tests.
    fprintf(stderr, "Too many arguments\n");
    return 1;
  }
  else {
    // Run all tests.
    struct testcase *test;
    int failures = 0;
    for (test = g_tests; test->test_name != NULL; test++) {
      failures += run_test_forked(test);
    }
    if (failures == 0) {
      printf("OK\n");
      return 0;
    }
    else {
      printf("%i FAILURE(S)\n", failures);
      return 1;
    }
  }
}
