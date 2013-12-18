// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_system_service.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/mock_drive_file_system.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/dummy_drive_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveSystemServiceTest : public testing::Test {
 public:
  DriveSystemServiceTest() :
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_system_(NULL),
      system_service_(NULL) {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    file_system_ = new MockDriveFileSystem;
    system_service_ = new DriveSystemService(profile_.get(),
                                             new google_apis::DummyDriveService,
                                             base::FilePath(),
                                             file_system_);
  }

  virtual void TearDown() OVERRIDE {
    delete system_service_;
    file_system_ = NULL;
    google_apis::test_util::RunBlockingPoolTask();
    profile_.reset();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;

  MockDriveFileSystem* file_system_;
  DriveSystemService* system_service_;
};

TEST_F(DriveSystemServiceTest, InitializeAndShutdown) {
  system_service_->Initialize();
  google_apis::test_util::RunBlockingPoolTask();
  system_service_->Shutdown();
}

}  // namespace drive
