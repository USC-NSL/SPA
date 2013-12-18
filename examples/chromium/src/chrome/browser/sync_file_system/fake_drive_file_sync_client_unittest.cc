// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/fake_drive_file_sync_client.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

void DidDownloadFile(google_apis::GDataErrorCode* error_out,
                     std::string* file_md5_out,
                     google_apis::GDataErrorCode error,
                     const std::string& file_md5,
                     int64 file_size,
                     const base::Time& updated_time) {
  *error_out = error;
  *file_md5_out = file_md5;
}

void DidGetChangeList(google_apis::GDataErrorCode* error_out,
                      scoped_ptr<google_apis::ResourceList>* change_list_out,
                      google_apis::GDataErrorCode error,
                      scoped_ptr<google_apis::ResourceList> change_list) {
  *error_out = error;
  *change_list_out = change_list.Pass();
}

void DidDeleteFile(google_apis::GDataErrorCode* error_out,
                   google_apis::GDataErrorCode error) {
  *error_out = error;
}

}  // namespace

TEST(FakeDriveFileSyncClientTest, ChangeSquashTest) {
  MessageLoop message_loop;
  FakeDriveFileSyncClient sync_client;
  std::string kParentResourceId("parent resource id");
  std::string kParentTitle("app-id");
  std::string kTitle1("title 1");
  std::string kTitle2("title 2");
  std::string kTitle3("title 3");
  std::string kResourceId1("resource id 1");
  std::string kResourceId2("resource id 2");
  std::string kMD5_1("md5 1");
  std::string kMD5_2("md5 2");
  std::string kMD5_3("md5 3");
  base::FilePath kTempFilePath(FILE_PATH_LITERAL("tmp_file"));

  sync_client.PushRemoteChange(
      kParentResourceId, kParentTitle,
      kTitle1, kResourceId1, kMD5_1, SYNC_FILE_TYPE_FILE, false /* deleted */);
  sync_client.PushRemoteChange(
      kParentResourceId, kParentTitle,
      kTitle1, kResourceId1, kMD5_1, SYNC_FILE_TYPE_FILE, false /* deleted */);
  sync_client.PushRemoteChange(
      kParentResourceId, kParentTitle,
      kTitle1, kResourceId1, kMD5_1, SYNC_FILE_TYPE_FILE, true /* deleted */);
  sync_client.PushRemoteChange(
      kParentResourceId, kParentTitle,
      kTitle2, kResourceId2, kMD5_2, SYNC_FILE_TYPE_FILE, false /* deleted */);
  sync_client.PushRemoteChange(
      kParentResourceId, kParentTitle,
      kTitle3, kResourceId2, kMD5_3, SYNC_FILE_TYPE_FILE, false /* deleted */);

  google_apis::GDataErrorCode error;
  std::string md5;
  sync_client.DownloadFile(
      kResourceId1, kMD5_1, kTempFilePath,
      base::Bind(DidDownloadFile, &error, &md5));
  message_loop.RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_NOT_FOUND, error);
  EXPECT_TRUE(md5.empty());

  scoped_ptr<google_apis::ResourceList> change_list;
  sync_client.ListChanges(
      0,
      base::Bind(&DidGetChangeList,
                 &error, &change_list));
  message_loop.RunUntilIdle();
  EXPECT_EQ(2u, change_list->entries().size());

  EXPECT_EQ(kResourceId1, change_list->entries()[0]->resource_id());
  EXPECT_TRUE(change_list->entries()[0]->deleted());

  EXPECT_EQ(kResourceId2, change_list->entries()[1]->resource_id());
  EXPECT_EQ(kMD5_3, change_list->entries()[1]->file_md5());
  EXPECT_FALSE(change_list->entries()[1]->deleted());
}

TEST(FakeDriveFileSyncClientTest, DeleteFile) {
  MessageLoop message_loop;
  FakeDriveFileSyncClient sync_client;
  std::string resource_id = "resource_id_to_be_deleted";
  sync_client.PushRemoteChange(
      "parent_id", "parent_title",
      "resource_title", resource_id, "resource_md5",
      SYNC_FILE_TYPE_FILE, false /* deleted */);

  google_apis::GDataErrorCode error = google_apis::HTTP_NOT_FOUND;
  sync_client.DeleteFile(
      resource_id,
      std::string(),
      base::Bind(&DidDeleteFile, &error));
  message_loop.RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_TRUE(sync_client.remote_resources().find(resource_id)->second.deleted);
}

}  // namespace sync_file_system
