// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/cache_metadata.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class CacheMetadataTest : public testing::Test {
 public:
  CacheMetadataTest() {}

  virtual void SetUp() OVERRIDE {
    // Create cache directories.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_paths_ = DriveCache::GetCachePaths(temp_dir_.path());
    ASSERT_TRUE(DriveCache::CreateCacheDirectories(cache_paths_));

    persistent_directory_ = cache_paths_[DriveCache::CACHE_TYPE_PERSISTENT];
    tmp_directory_ = cache_paths_[DriveCache::CACHE_TYPE_TMP];
    outgoing_directory_ = cache_paths_[DriveCache::CACHE_TYPE_OUTGOING];
  }

  virtual void TearDown() OVERRIDE {
    metadata_.reset();
  }

  // Sets up the CacheMetadata object.
  void SetUpCacheMetadata() {
    metadata_ = CacheMetadata::CreateCacheMetadata(NULL);
    ASSERT_TRUE(metadata_->Initialize(cache_paths_));
  }

  // Sets up the cache directories with various files, including stale
  // symbolic links etc. Should be called before SetUpCacheMetadata().
  void SetUpCacheWithVariousFiles() {
    // Create some files in persistent directory.
    //
    CreateFile(persistent_directory_.AppendASCII("id_foo.md5foo"));
    CreateFile(persistent_directory_.AppendASCII("id_bar.local"));
    // "id_baz" is dirty but does not have a symlink in outgoing
    // directory. This file should be removed.
    CreateFile(persistent_directory_.AppendASCII("id_baz.local"));
    // "id_symlink" is invalid, as symlink is not allowed here. This should
    // be removed.
    CreateSymbolicLink(base::FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull),
                       persistent_directory_.AppendASCII("id_symlink"));

    // Create some files in tmp directory.
    //
    CreateFile(tmp_directory_.AppendASCII("id_qux.md5qux"));
    // "id_quux" is invalid as we shouldn't have a dirty file in "tmp".
    CreateFile(tmp_directory_.AppendASCII("id_quux.local"));
    // "id_symlink_tmp" is invalid, as symlink is not allowed here. This
    // should be removed.
    CreateSymbolicLink(base::FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull),
                       tmp_directory_.AppendASCII("id_symlink_tmp"));

    // Create symbolic links in outgoing directory.
    //
    // "id_bar" is dirty and committed.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_bar.local"),
                       outgoing_directory_.AppendASCII("id_bar"));
    // "id_foo" is not dirty. This symlink should be removed.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_foo.md5foo"),
                       outgoing_directory_.AppendASCII("id_foo"));
  }

  // Create a file at |file_path|.
  void CreateFile(const base::FilePath& file_path) {
    const std::string kFoo = "foo";
    ASSERT_TRUE(file_util::WriteFile(file_path, kFoo.data(), kFoo.size()))
        << ": " << file_path.value();
  }

  // Create an symlink to |target| at |symlink|.
  void CreateSymbolicLink(const base::FilePath& target,
                          const base::FilePath& symlink) {
    ASSERT_TRUE(file_util::CreateSymbolicLink(target, symlink))
        << ": " << target.value() << ": " << symlink.value();
  }

 protected:
  // Helper function to insert an item with key |resource_id| into |cache_map|.
  // |md5| and |cache_state| are used to create the value CacheEntry.
  void InsertIntoMap(CacheMetadata::CacheMap* cache_map,
                     const std::string& resource_id,
                     const CacheEntry& cache_entry) {
    cache_map->insert(std::make_pair(
        resource_id, cache_entry));
  }

  // Adds all entries in |cache_map| to the metadata storage.
  void AddAllMapEntries(const CacheMetadata::CacheMap& cache_map) {
    for (CacheMetadata::CacheMap::const_iterator iter = cache_map.begin();
         iter != cache_map.end(); ++iter) {
      metadata_->AddOrUpdateCacheEntry(iter->first, iter->second);
    }
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<CacheMetadata> metadata_;
  std::vector<base::FilePath> cache_paths_;
  base::FilePath persistent_directory_;
  base::FilePath tmp_directory_;
  base::FilePath outgoing_directory_;
};

// Test all the methods of CacheMetadata except for
// RemoveTemporaryFiles.
TEST_F(CacheMetadataTest, CacheTest) {
  SetUpCacheMetadata();

  // Save an initial entry.
  std::string test_resource_id("test_resource_id");
  std::string test_file_md5("test_file_md5");
  {
    CacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_present(true);
    new_cache_entry.set_is_persistent(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Test that the entry can be retrieved.
  CacheEntry cache_entry;
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_persistent());

  // Empty md5 should also work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // resource_id doesn't exist.
  EXPECT_FALSE(metadata_->GetCacheEntry(
      "not_found_resource_id", std::string(), &cache_entry));

  // md5 doesn't match.
  EXPECT_FALSE(metadata_->GetCacheEntry(
      test_resource_id, "mismatch_md5", &cache_entry));

  // Update all attributes.
  test_file_md5 = "test_file_md5_2";
  {
    CacheEntry updated_cache_entry;
    updated_cache_entry.set_md5(test_file_md5);
    updated_cache_entry.set_is_pinned(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, updated_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_pinned());

  // Empty m5 should work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Test dirty cache.
  test_file_md5 = "test_file_md5_3";
  {
    CacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_dirty(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_dirty());

  // Empty md5 should work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Mismatched md5 should also work for dirty entries.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, "mismatch_md5", &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Remove the entry.
  metadata_->RemoveCacheEntry(test_resource_id);
  EXPECT_FALSE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));

  // Add another one.
  test_resource_id = "test_resource_id_2";
  test_file_md5 = "test_file_md5_4";
  {
    CacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_present(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());
}

TEST_F(CacheMetadataTest, CorruptDB) {
  using file_util::PathExists;
  using file_util::IsLink;
  SetUpCacheWithVariousFiles();

  const base::FilePath db_path =
      cache_paths_[DriveCache::CACHE_TYPE_META].Append(
          CacheMetadata::kCacheMetadataDBPath);

  // Write a bogus file.
  std::string text("Hello world");
  file_util::WriteFile(db_path, text.c_str(), text.length());

  // Some files are removed during cache initialization. Make sure these
  // exist beforehand.
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_baz.local")));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_quux.local")));
  EXPECT_TRUE(IsLink(persistent_directory_.AppendASCII("id_symlink")));
  EXPECT_TRUE(IsLink(tmp_directory_.AppendASCII("id_symlink_tmp")));

  SetUpCacheMetadata();

  // Check contents in "persistent" directory.
  //
  // "id_foo" is moved to temporary directory.
  CacheEntry cache_entry;
  ASSERT_TRUE(metadata_->GetCacheEntry("id_foo", "md5foo", &cache_entry));
  EXPECT_EQ("md5foo", cache_entry.md5());
  EXPECT_FALSE(cache_entry.is_persistent());
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT),
      cache_entry));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_foo.md5foo")));
  // The invalid symlink in "outgoing" should be removed.
  EXPECT_FALSE(PathExists(outgoing_directory_.AppendASCII("id_foo")));

  // "id_bar" is present and dirty.
  ASSERT_TRUE(metadata_->GetCacheEntry("id_bar", "", &cache_entry));
  EXPECT_EQ("local", cache_entry.md5());
  EXPECT_EQ(DriveCache::CACHE_TYPE_PERSISTENT,
            DriveCache::GetSubDirectoryType(cache_entry));
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT |
                              test_util::TEST_CACHE_STATE_DIRTY |
                              test_util::TEST_CACHE_STATE_PERSISTENT),
      cache_entry));
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_bar.local")));
  EXPECT_TRUE(PathExists(outgoing_directory_.AppendASCII("id_bar")));

  // "id_baz" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_baz", "", &cache_entry));
  EXPECT_FALSE(PathExists(persistent_directory_.AppendASCII("id_baz.local")));

  // "id_symlink" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_symlink", "", &cache_entry));
  EXPECT_FALSE(PathExists(persistent_directory_.AppendASCII("id_symlink")));

  // Check contents in "tmp" directory.
  //
  // "id_qux" is just present in tmp directory.
  ASSERT_TRUE(metadata_->GetCacheEntry("id_qux", "md5qux", &cache_entry));
  EXPECT_EQ("md5qux", cache_entry.md5());
  EXPECT_EQ(DriveCache::CACHE_TYPE_TMP,
            DriveCache::GetSubDirectoryType(cache_entry));
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT),
      cache_entry));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_qux.md5qux")));

  // "id_quux" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_quux", "md5qux", &cache_entry));

  // "id_symlink_tmp" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_symlink_tmp", "", &cache_entry));

  // "id_dangling" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_dangling", "", &cache_entry));

  // "id_outside" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_outside", "", &cache_entry));

  // "id_not_symlink" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_not_symlink", "", &cache_entry));
}

// Test CacheMetadata::RemoveTemporaryFiles.
TEST_F(CacheMetadataTest, RemoveTemporaryFiles) {
  SetUpCacheMetadata();

  CacheMetadata::CacheMap cache_map;
  {
    CacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    InsertIntoMap(&cache_map, "<resource_id_1>", cache_entry);
  }
  {
    CacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    cache_entry.set_is_persistent(true);
    InsertIntoMap(&cache_map, "<resource_id_2>", cache_entry);
  }
  {
    CacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    cache_entry.set_is_persistent(true);
    InsertIntoMap(&cache_map, "<resource_id_3>", cache_entry);
  }
  {
    CacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    InsertIntoMap(&cache_map, "<resource_id_4>", cache_entry);
  }

  AddAllMapEntries(cache_map);
  metadata_->RemoveTemporaryFiles();
  // resource 1 and 4 should be gone, as these are temporary.
  CacheEntry cache_entry;
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_1>", "", &cache_entry));
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_2>", "", &cache_entry));
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_3>", "", &cache_entry));
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_4>", "", &cache_entry));
}

// Don't use TEST_F, as we don't want SetUp() and TearDown() for this test.
TEST(CacheMetadataExtraTest, CannotOpenDB) {
  // Create nonexistent cache paths, so the initialization fails due to the
  // failure of opening the DB.
  std::vector<base::FilePath> cache_paths =
      DriveCache::GetCachePaths(
          base::FilePath::FromUTF8Unsafe("/somewhere/nonexistent"));

  scoped_ptr<CacheMetadata> metadata =
      CacheMetadata::CreateCacheMetadata(NULL);
  EXPECT_FALSE(metadata->Initialize(cache_paths));
}

}  // namespace drive
