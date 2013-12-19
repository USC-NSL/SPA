// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__x86_64__)
# include "trusted_thread_x86_64.S"
#elif defined(__i386__)
# include "trusted_thread_i386.S"
#else
# error "Unsupported target platform"
#endif

        // Tell Linux not to disable no-execute protection for the process.
        .section .note.GNU-stack,"",@progbits
