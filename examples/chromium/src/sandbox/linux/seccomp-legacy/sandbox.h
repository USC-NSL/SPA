// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_H__
#define SANDBOX_H__

// See sandbox_impl.h for detailed comments on this API
extern "C" int  SupportsSeccompSandbox(int proc);
extern "C" void SeccompSandboxSetProcFd(int proc);
extern "C" void StartSeccompSandbox();

#endif // SANDBOX_H__
