// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DIRECTORY_CHANGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DIRECTORY_CHANGE_OBSERVER_H_

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drive {

// Mock for FileSystemObserver::OnDirectoryChanged().
class MockDirectoryChangeObserver : public FileSystemObserver {
 public:
  MockDirectoryChangeObserver();
  virtual ~MockDirectoryChangeObserver();

  // FileSystemObserver overrides.
  MOCK_METHOD1(OnDirectoryChanged, void(const base::FilePath& directory_path));
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DIRECTORY_CHANGE_OBSERVER_H_
