// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Fails on Windows, but we're not really supporting windows yet.
#if defined(USE_ASH) && defined(USE_AURA) && !defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Input) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("input")) << message_;
}
#endif
