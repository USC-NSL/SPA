// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker_chromeos.h"

#include "base/bind.h"
#include "base/i18n/file_util_icu.h"
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::DownloadItem;
using content::DownloadManager;

DownloadFilePickerChromeOS::DownloadFilePickerChromeOS() {
}

DownloadFilePickerChromeOS::~DownloadFilePickerChromeOS() {
}

void DownloadFilePickerChromeOS::InitSuggestedPath(DownloadItem* item,
                                                   const base::FilePath& path) {
  // For Drive downloads, we should pass the drive path instead of the temporary
  // file path.
  Profile* profile =
      Profile::FromBrowserContext(download_manager_->GetBrowserContext());
  drive::DownloadHandler* drive_download_handler =
      drive::DownloadHandler::GetForProfile(profile);
  base::FilePath suggested_path = path;
  if (drive_download_handler && drive_download_handler->IsDriveDownload(item))
    suggested_path = drive_download_handler->GetTargetPath(item);

  DownloadFilePicker::InitSuggestedPath(item, suggested_path);
}

void DownloadFilePickerChromeOS::FileSelected(
    const base::FilePath& selected_path,
    int index,
    void* params) {
  FileSelectedWithExtraInfo(
      ui::SelectedFileInfo(selected_path, selected_path),
      index,
      params);
}

void DownloadFilePickerChromeOS::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file_info,
    int index,
    void* params) {
  base::FilePath path = file_info.file_path;
  file_util::NormalizeFileNameEncoding(&path);

  // Need to do this before we substitute with a temporary path. Otherwise we
  // won't be able to detect path changes.
  RecordFileSelected(path);

  if (download_manager_) {
    Profile* profile =
        Profile::FromBrowserContext(download_manager_->GetBrowserContext());
    drive::DownloadHandler* drive_download_handler =
        drive::DownloadHandler::GetForProfile(profile);
    if (drive_download_handler) {
      DownloadItem* download = download_manager_->GetDownload(download_id_);
      drive_download_handler->SubstituteDriveDownloadPath(
          path, download,
          base::Bind(&DownloadFilePickerChromeOS::OnFileSelected,
                     base::Unretained(this)));
    } else {
      OnFileSelected(path);
    }
  } else {
    OnFileSelected(base::FilePath());
  }
  // The OnFileSelected() call deletes |this|
}
