// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox_impl.h"
#include "system_call_table.h"

void runTrustedThread(void* args, int segment_selector)
  asm("playground$runTrustedThread") INTERNAL;

namespace playground {

void Sandbox::createTrustedThread(SecureMem::Args* secureMem) {
  SecureMem::Args args                  = { { { { { 0 } } } } };
  args.self                             = &args;
  args.newSecureMem                     = secureMem;

#if defined(__i386__)
  struct user_desc u;
  u.entry_number    = (typeof u.entry_number)-1;
  u.base_addr       = 0;
  u.limit           = 0xfffff;
  u.seg_32bit       = 1;
  u.contents        = 0;
  u.read_exec_only  = 0;
  u.limit_in_pages  = 1;
  u.seg_not_present = 0;
  u.useable         = 1;
  SysCalls sys;
  if (sys.set_thread_area(&u) < 0) {
    die("Cannot set up thread local storage");
  }
  int segment_selector = 8 * u.entry_number + 3;
#else
  int segment_selector = 0;
#endif

  runTrustedThread(&args, segment_selector);
}

} // namespace
