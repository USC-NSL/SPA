// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include <string>

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;
using google_apis::ResourceEntry;
using google_apis::GDataErrorCode;

namespace drive {
namespace file_system {

namespace {

const char kMimeTypeOctetStream[] = "application/octet-stream";

// Copies a file from |src_file_path| to |dest_file_path| on the local
// file system using file_util::CopyFile.
// Returns FILE_ERROR_OK on success or FILE_ERROR_FAILED otherwise.
FileError CopyLocalFileOnBlockingPool(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path) {
  return file_util::CopyFile(src_file_path, dest_file_path) ?
      FILE_ERROR_OK : FILE_ERROR_FAILED;
}

// Checks if a local file at |local_file_path| is a JSON file referencing a
// hosted document on blocking pool, and if so, gets the resource ID of the
// document.
std::string GetDocumentResourceIdOnBlockingPool(
    const base::FilePath& local_file_path) {
  std::string result;
  if (ResourceEntry::HasHostedDocumentExtension(local_file_path)) {
    std::string error;
    DictionaryValue* dict_value = NULL;
    JSONFileValueSerializer serializer(local_file_path);
    scoped_ptr<Value> value(serializer.Deserialize(NULL, &error));
    if (value.get() && value->GetAsDictionary(&dict_value))
      dict_value->GetString("resource_id", &result);
  }
  return result;
}

}  // namespace

// CopyOperation::StartFileUploadParams implementation.
struct CopyOperation::StartFileUploadParams {
  StartFileUploadParams(const base::FilePath& in_local_file_path,
                        const base::FilePath& in_remote_file_path,
                        const FileOperationCallback& in_callback)
      : local_file_path(in_local_file_path),
        remote_file_path(in_remote_file_path),
        callback(in_callback) {}

  const base::FilePath local_file_path;
  const base::FilePath remote_file_path;
  const FileOperationCallback callback;
};

CopyOperation::CopyOperation(
    JobScheduler* job_scheduler,
    DriveFileSystemInterface* drive_file_system,
    internal::ResourceMetadata* metadata,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OperationObserver* observer)
  : job_scheduler_(job_scheduler),
    drive_file_system_(drive_file_system),
    metadata_(metadata),
    blocking_task_runner_(blocking_task_runner),
    observer_(observer),
    move_operation_(new MoveOperation(job_scheduler,
                                      metadata,
                                      observer)),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CopyOperation::~CopyOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CopyOperation::Copy(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const FileOperationCallback& callback) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  DCHECK(!callback.is_null());

  metadata_->GetEntryInfoPairByPaths(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&CopyOperation::CopyAfterGetEntryInfoPair,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void CopyOperation::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_file_system_->GetFileByPath(
      remote_src_file_path,
      base::Bind(&CopyOperation::OnGetFileCompleteForTransferFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_dest_file_path,
                 callback));
}

void CopyOperation::OnGetFileCompleteForTransferFile(
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // GetFileByPath downloads the file from Drive to a local cache, which is then
  // copied to the actual destination path on the local file system using
  // CopyLocalFileOnBlockingPool.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&CopyLocalFileOnBlockingPool,
                 local_file_path,
                 local_dest_file_path),
      callback);
}

void CopyOperation::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  metadata_->GetEntryInfoByPath(
      remote_dest_file_path.DirName(),
      base::Bind(
          &CopyOperation::TransferFileFromLocalToRemoteAfterGetEntryInfo,
          weak_ptr_factory_.GetWeakPtr(),
          local_src_file_path,
          remote_dest_file_path,
          callback));
}

void CopyOperation::TransferRegularFile(
    const base::FilePath& local_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* content_type = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&net::GetMimeTypeFromFile,
                 remote_dest_file_path,
                 base::Unretained(content_type)),
      base::Bind(&CopyOperation::StartFileUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 StartFileUploadParams(local_file_path,
                                       remote_dest_file_path,
                                       callback),
                 base::Owned(content_type)));
}

void CopyOperation::CopyHostedDocumentToDirectory(
    const base::FilePath& dir_path,
    const std::string& resource_id,
    const base::FilePath::StringType& new_name,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  job_scheduler_->CopyHostedDocument(
      resource_id,
      base::FilePath(new_name).AsUTF8Unsafe(),
      base::Bind(&CopyOperation::OnCopyHostedDocumentCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 dir_path,
                 callback));
}

void CopyOperation::OnCopyHostedDocumentCompleted(
    const base::FilePath& dir_path,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(resource_entry);

  // The entry was added in the root directory on the server, so we should
  // first add it to the root to mirror the state and then move it to the
  // destination directory by MoveEntryFromRootDirectory().
  metadata_->AddEntry(ConvertResourceEntryToDriveEntryProto(*resource_entry),
                      base::Bind(&CopyOperation::MoveEntryFromRootDirectory,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 dir_path,
                                 callback));
}

void CopyOperation::MoveEntryFromRootDirectory(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK_EQ(util::kDriveMyDriveRootPath, file_path.DirName().value());

  // Return if there is an error or |dir_path| is the root directory.
  if (error != FILE_ERROR_OK ||
      directory_path == util::GetDriveMyDriveRootPath()) {
    callback.Run(error);
    return;
  }

  move_operation_->Move(file_path,
                        directory_path.Append(file_path.BaseName()),
                        callback);
}

void CopyOperation::CopyAfterGetEntryInfoPair(
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  if (result->first.error != FILE_ERROR_OK) {
    callback.Run(result->first.error);
    return;
  } else if (result->second.error != FILE_ERROR_OK) {
    callback.Run(result->second.error);
    return;
  }

  scoped_ptr<DriveEntryProto> src_file_proto = result->first.proto.Pass();
  scoped_ptr<DriveEntryProto> dest_parent_proto = result->second.proto.Pass();

  if (!dest_parent_proto->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  } else if (src_file_proto->file_info().is_directory()) {
    // TODO(kochi): Implement copy for directories. In the interim,
    // we handle recursive directory copy in the file manager.
    // crbug.com/141596
    callback.Run(FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (src_file_proto->file_specific_info().is_hosted_document()) {
    CopyHostedDocumentToDirectory(
        dest_file_path.DirName(),
        src_file_proto->resource_id(),
        // Drop the document extension, which should not be in the title.
        dest_file_path.BaseName().RemoveExtension().value(),
        callback);
    return;
  }

  // TODO(kochi): Reimplement this once the server API supports
  // copying of regular files directly on the server side. crbug.com/138273
  const base::FilePath& src_file_path = result->first.path;
  drive_file_system_->GetFileByPath(
      src_file_path,
      base::Bind(&CopyOperation::OnGetFileCompleteForCopy,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void CopyOperation::OnGetFileCompleteForCopy(
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    const std::string& unused_mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // This callback is only triggered for a regular file via Copy().
  DCHECK_EQ(REGULAR_FILE, file_type);
  TransferRegularFile(local_file_path, remote_dest_file_path, callback);
}

void CopyOperation::StartFileUpload(const StartFileUploadParams& params,
                                    const std::string* content_type,
                                    bool got_content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  // Make sure the destination directory exists.
  metadata_->GetEntryInfoByPath(
      params.remote_file_path.DirName(),
      base::Bind(&CopyOperation::StartFileUploadAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 got_content_type ? *content_type : kMimeTypeOctetStream));
}

void CopyOperation::StartFileUploadAfterGetEntryInfo(
    const StartFileUploadParams& params,
    const std::string& content_type,
    FileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (entry_proto.get() && !entry_proto->file_info().is_directory())
    error = FILE_ERROR_NOT_A_DIRECTORY;

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }
  DCHECK(entry_proto.get());

  job_scheduler_->UploadNewFile(
      entry_proto->resource_id(),
      params.remote_file_path,
      params.local_file_path,
      params.remote_file_path.BaseName().value(),
      content_type,
      DriveClientContext(USER_INITIATED),
      base::Bind(&CopyOperation::OnTransferCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 params.callback));
}

void CopyOperation::OnTransferCompleted(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode error,
    const base::FilePath& drive_path,
    const base::FilePath& file_path,
    scoped_ptr<ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == google_apis::HTTP_SUCCESS && resource_entry.get()) {
    drive_file_system_->AddUploadedFile(resource_entry.Pass(),
                                        file_path,
                                        callback);
  } else {
    callback.Run(util::GDataToFileError(error));
  }
}

void CopyOperation::TransferFileFromLocalToRemoteAfterGetEntryInfo(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry_proto.get());
  if (!entry_proto->file_info().is_directory()) {
    // The parent of |remote_dest_file_path| is not a directory.
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&GetDocumentResourceIdOnBlockingPool, local_src_file_path),
      base::Bind(&CopyOperation::TransferFileForResourceId,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_src_file_path,
                 remote_dest_file_path,
                 callback));
}

void CopyOperation::TransferFileForResourceId(
    const base::FilePath& local_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback,
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (resource_id.empty()) {
    // If |resource_id| is empty, upload the local file as a regular file.
    TransferRegularFile(local_file_path, remote_dest_file_path, callback);
    return;
  }

  // Otherwise, copy the document on the server side and add the new copy
  // to the destination directory (collection).
  CopyHostedDocumentToDirectory(
      remote_dest_file_path.DirName(),
      resource_id,
      // Drop the document extension, which should not be
      // in the document title.
      remote_dest_file_path.BaseName().RemoveExtension().value(),
      callback);
}

}  // namespace file_system
}  // namespace drive
