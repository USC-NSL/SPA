// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__x86_64__)
# include "fault_handler_x86_64.S"
#elif defined(__i386__)
# include "fault_handler_i386.S"
#else
# error "Unsupported target platform"
#endif

        .pushsection ".rodata"
#ifndef NDEBUG
    100:.asciz "RDTSC(P): Executing handler\n"
    200:.asciz "INT $0x0: Executing handler\n"
#endif
        .popsection

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
