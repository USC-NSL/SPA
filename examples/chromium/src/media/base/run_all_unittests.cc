// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "media/base/media.h"

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv) {}
  virtual ~TestSuiteNoAtExit() {}
 protected:
  virtual void Initialize() OVERRIDE;
};

void TestSuiteNoAtExit::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();
  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibraryForTesting();
}

int main(int argc, char** argv) {
  return TestSuiteNoAtExit(argc, argv).Run();
}
