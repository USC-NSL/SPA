// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_PROXY_H_

#include "chrome/browser/chromeos/drive/file_errors.h"
#include "webkit/fileapi/remote_file_system_proxy.h"

namespace fileapi {
class FileSystemURL;
}

namespace drive {

class DriveEntryProto;
class DriveFileSystemInterface;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

// Implementation of File API's remote file system proxy for Drive file system.
class FileSystemProxy : public fileapi::RemoteFileSystemProxyInterface {
 public:
  using fileapi::RemoteFileSystemProxyInterface::OpenFileCallback;

  // |file_system| is the DriveFileSystem instance owned by DriveSystemService.
  explicit FileSystemProxy(DriveFileSystemInterface* file_system);

  // Detaches this instance from |file_system_|.
  // Method calls may result in no-op after calling this method.
  // This method must be called on UI thread.
  void DetachFromFileSystem();

  // fileapi::RemoteFileSystemProxyInterface overrides.
  virtual void GetFileInfo(
      const fileapi::FileSystemURL& url,
      const fileapi::FileSystemOperation::GetMetadataCallback&
          callback) OVERRIDE;
  virtual void Copy(
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void Move(
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void ReadDirectory(const fileapi::FileSystemURL& url,
     const fileapi::FileSystemOperation::ReadDirectoryCallback&
         callback) OVERRIDE;
  virtual void Remove(
      const fileapi::FileSystemURL& url, bool recursive,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateDirectory(
      const fileapi::FileSystemURL& file_url,
      bool exclusive,
      bool recursive,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateFile(
      const fileapi::FileSystemURL& file_url,
      bool exclusive,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void Truncate(
      const fileapi::FileSystemURL& file_url, int64 length,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;
  virtual void CreateSnapshotFile(
      const fileapi::FileSystemURL& url,
      const fileapi::FileSystemOperation::SnapshotFileCallback&
      callback) OVERRIDE;
  virtual void CreateWritableSnapshotFile(
      const fileapi::FileSystemURL& url,
      const fileapi::WritableSnapshotFile& callback) OVERRIDE;
  virtual void OpenFile(
      const fileapi::FileSystemURL& url,
      int file_flags,
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback) OVERRIDE;
  virtual void NotifyCloseFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void TouchFile(
      const fileapi::FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const fileapi::FileSystemOperation::StatusCallback& callback)
          OVERRIDE;

 protected:
  virtual ~FileSystemProxy();

 private:
  // Checks if a given |url| belongs to this file system. If it does,
  // the call will return true and fill in |file_path| with a file path of
  // a corresponding element within this file system.
  static bool ValidateUrl(const fileapi::FileSystemURL& url,
                          base::FilePath* file_path);

  // Helper method to call methods of DriveFilesSystem. This method aborts
  // method calls in case DetachFromFileSystem() has been called.
  void CallDriveFileSystemMethodOnUIThread(const base::Closure& method_call);

  // Used to implement CallDriveFileSystemMethodOnUIThread.
  void CallDriveFileSystemMethodOnUIThreadInternal(
      const base::Closure& method_call);

  // Helper callback for relaying reply for status callbacks to the
  // calling thread.
  void OnStatusCallback(
      const fileapi::FileSystemOperation::StatusCallback& callback,
      FileError error);

  // Helper callback for relaying reply for metadata retrieval request to the
  // calling thread.
  void OnGetMetadata(
      const base::FilePath& file_path,
      const fileapi::FileSystemOperation::GetMetadataCallback&
          callback,
      FileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Helper callback for relaying reply for GetEntryInfoByPath() to the
  // calling thread.
  void OnGetEntryInfoByPath(
      const base::FilePath& entry_path,
      const fileapi::FileSystemOperation::SnapshotFileCallback&
          callback,
      FileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Helper callback for relaying reply for ReadDirectory() to the calling
  // thread.
  void OnReadDirectory(
      const fileapi::FileSystemOperation::ReadDirectoryCallback&
          callback,
      FileError error,
      bool hide_hosted_documents,
      scoped_ptr<DriveEntryProtoVector> proto_entries);

  // Helper callback for relaying reply for CreateWritableSnapshotFile() to
  // the calling thread.
  void OnCreateWritableSnapshotFile(
      const base::FilePath& virtual_path,
      const fileapi::WritableSnapshotFile& callback,
      FileError result,
      const base::FilePath& local_path);

  // Helper callback for closing the local cache file and committing the dirty
  // flag. This is triggered when the callback for CreateWritableSnapshotFile
  // released the refcounted reference to the file.
  void CloseWritableSnapshotFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_path);

  // Invoked during Truncate() operation. This is called when a local modifiable
  // cache is ready for truncation.
  void OnFileOpenedForTruncate(
      const base::FilePath& virtual_path,
      int64 length,
      const fileapi::FileSystemOperation::StatusCallback& callback,
      FileError open_result,
      const base::FilePath& local_cache_path);

  // Invoked during Truncate() operation. This is called when the truncation of
  // a local cache file is finished on FILE thread.
  void DidTruncate(
      const base::FilePath& virtual_path,
      const fileapi::FileSystemOperation::StatusCallback& callback,
      base::PlatformFileError truncate_result);

  // Invoked during OpenFile() operation when truncate or write flags are set.
  // This is called when a local modifiable cached file is ready for such
  // operation.
  void OnOpenFileForWriting(
      int file_flags,
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback,
      FileError file_error,
      const base::FilePath& local_cache_path);

  // Invoked during OpenFile() operation when file create flags are set.
  void OnCreateFileForOpen(
      const base::FilePath& file_path,
      int file_flags,
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback,
      FileError file_error);

  // Invoked during OpenFile() operation when base::PLATFORM_FILE_OPEN_TRUNCATED
  // flag is set. This is called when the truncation of a local cache file is
  // finished on FILE thread.
  void OnOpenAndTruncate(
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback,
      base::PlatformFile* platform_file,
      base::PlatformFileError* truncate_result);

  DriveFileSystemInterface* file_system_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_PROXY_H_
