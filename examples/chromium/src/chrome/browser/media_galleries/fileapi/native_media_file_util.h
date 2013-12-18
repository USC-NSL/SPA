// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/isolated_file_util.h"

namespace chrome {

// This class handles native file system operations with media type filtering
// which is passed to each method via fileapi::FileSystemOperationContext as
// MediaPathFilter.
class NativeMediaFileUtil : public fileapi::IsolatedFileUtil {
 public:
  NativeMediaFileUtil();

  virtual base::PlatformFileError CreateOrOpen(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual base::PlatformFileError EnsureFileExists(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url, bool* created) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& root_url) OVERRIDE;
  virtual base::PlatformFileError Touch(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::PlatformFileError Truncate(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError CopyInForeignFile(
        fileapi::FileSystemOperationContext* context,
        const base::FilePath& src_file_path,
        const fileapi::FileSystemURL& dest_url) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError GetFileInfo(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_path) OVERRIDE;

 private:
  // Like GetLocalFilePath(), but always take media_path_filter() into
  // consideration. If the media_path_filter() check fails, return
  // PLATFORM_FILE_ERROR_SECURITY. |local_file_path| does not have to exist.
  base::PlatformFileError GetFilteredLocalFilePath(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);

  // Like GetLocalFilePath(), but if the file does not exist, then return
  // |failure_error|.
  // If |local_file_path| is a file, then take media_path_filter() into
  // consideration.
  // If the media_path_filter() check fails, return |failure_error|.
  // If |local_file_path| is a directory, return PLATFORM_FILE_OK.
  base::PlatformFileError GetFilteredLocalFilePathForExistingFileOrDirectory(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& file_system_url,
      base::PlatformFileError failure_error,
      base::FilePath* local_file_path);

  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtil);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
