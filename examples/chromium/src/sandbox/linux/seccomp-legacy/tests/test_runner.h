// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_RUNNER_H__
#define TEST_RUNNER_H__

#include <errno.h>
#include <stdio.h>


void intend_exit_status(int val, bool is_signal);
void add_test_case(const char *test_name, void (*test_func)());


#define TEST(name) \
  void name(); \
  static void __attribute__((constructor)) add_##name() { \
    add_test_case(#name, name); \
  } \
  void name()


#ifdef DEBUG
#define MSG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define MSG(fmt, ...) do { } while (0)
#endif


#pragma GCC diagnostic ignored "-Wunused-value"

// Checks that "expr" evaluates to "true". Returns value of "expr".
#define CHECK(expr)                                                           \
  ({ typeof (expr) check_res = (expr);                                        \
     if (!check_res) {                                                        \
       fprintf(stderr, "%s:%d: Check failed in \"%s\": %s\n",                 \
               __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);               \
       _exit(1);                                                              \
     }                                                                        \
     check_res;                                                               \
  })

// Checks that "expr" evaluates to "true". Prints "errno" value on failure.
// Returns value of "expr".
#define CHECK_SUCCEEDS(expr)                                                  \
  ({ typeof (expr) check_res = (expr);                                        \
     if (!check_res) {                                                        \
       char errmsg[80];                                                       \
       fprintf(stderr, "%s:%d: Check failed in \"%s\": %s: \"%s\"\n",         \
               __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr,                \
               strerror_r(errno, errmsg, sizeof(errmsg)));                    \
       _exit(1);                                                              \
     }                                                                        \
     check_res;                                                               \
  })

// Checks that "expr" evaluates to "true", or that is sets "errno" to
// "exp_errno". Prints "errno" value on failure. Returns value of "expr".
#define CHECK_MAYFAIL(expr, exp_errno)                                        \
  ({ typeof (expr) check_res = (expr);                                        \
    if (!check_res && errno != (exp_errno)) {                                 \
      char errmsg1[80], errmsg2[80];                                          \
      fprintf(stderr, "%s:%d: Check failed in \"%s\": %s: expected \"%s\" "   \
              "but was \"%s\"\n",                                             \
              __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr,                 \
              strerror_r(exp_errno, errmsg1, sizeof(errmsg1)),                \
              strerror_r(errno, errmsg2, sizeof(errmsg2)));                   \
      _exit(1);                                                               \
    }                                                                         \
    check_res;                                                                \
  })

// Checks that "expr" evaluates to "-1" and that it sets "errno" to
// "exp_errno". Prints actual "errno" value otherwise. Returns value of "expr".
#define CHECK_ERRNO(expr, exp_errno)                                          \
  ({ typeof (expr) check_res = (expr);                                        \
    if (check_res != (typeof check_res)-1) {                                  \
       fprintf(stderr, "%s:%d: Check unexpectedly succeeded in \"%s\": %s\n", \
               __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);               \
       _exit(1);                                                              \
    } else if (errno != (exp_errno)) {                                        \
      char errmsg1[80], errmsg2[80];                                          \
      fprintf(stderr, "%s:%d: Check failed in \"%s\": %s: expected \"%s\" "   \
              "but was \"%s\"\n",                                             \
              __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr,                 \
              strerror_r(exp_errno, errmsg1, sizeof(errmsg1)),                \
              strerror_r(errno, errmsg2, sizeof(errmsg2)));                   \
      _exit(1);                                                               \
    }                                                                         \
    check_res;                                                                \
  })


#endif
