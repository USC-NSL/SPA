// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSCALL_ENTRYPOINT_H__
#define SYSCALL_ENTRYPOINT_H__

#ifndef INTERNAL
#define INTERNAL __attribute__((visibility("internal")))
#endif

extern "C" {

// This is the entrypoint which is called by the untrusted code, trying to
// make a system call.
void syscallEntryPointWithFrame()
                         asm("playground$syscallEntryPointWithFrame") INTERNAL;

// Normally, when we rewrite code to intercept a system call, we set up
// a special call frame that can later help in identifying call stacks.
// In particular, we keep track both of the actual address that we need to
// return to (this is still part of our intercepted code), and we keep track
// of the address where the un-modified system call would have returned to.
// Furthermore, we set up a chain of frame pointers that ends in 0xDEADBEEF.
//
// Programs such as breakpad can use this information to remove all traces
// of the seccomp sandbox from the core files that they generate. This helps
// when debugging with "gdb", which otherwise gets confused by seeing
// executable code outside of the .text segment.
//
// There are a few special situations, though, were we want to call the
// system call entrypoint without having had a chance to initialize our special
// call frame. This happens, whenever we don't actually need to instrument
// instructions (e.g. because there already is a perfectly good "CALL"
// instructions). In that case, we call the syscallEntryPointNoFrame()
// function. It sets up the extra return address in the stack frame prior to
// falling through to the syscallEntryPointWithFrame() function.
void syscallEntryPointNoFrame()
                           asm("playground$syscallEntryPointNoFrame") INTERNAL;

}

#endif // SYSCALL_ENTRYPOINT_H__
