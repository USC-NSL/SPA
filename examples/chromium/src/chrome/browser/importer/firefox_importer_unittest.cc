// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox_importer_unittest_utils.h"
#include "chrome/browser/importer/nss_decryptor.h"
#include "chrome/common/chrome_paths.h"

// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_NSS(x) DISABLED_##x
#else
#define MAYBE_NSS(x) x
#endif

// The following 2 tests require the use of the NSSDecryptor, on OSX this needs
// to run in a separate process, so we use a proxy object so we can share the
// same test between platforms.
TEST(FirefoxImporterTest, MAYBE_NSS(Firefox2NSS3Decryptor)) {
  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#if defined(OS_MACOSX)
  nss_path = nss_path.AppendASCII("firefox2_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox2_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox2_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  EXPECT_EQ(ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECBJ"
                              "M63MpT9rtBAjMCm7qo/EhlA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECN9"
                              "OQ5ZFmhb8BAiFo1Z+fUvaIQ=="));
}

TEST(FirefoxImporterTest, MAYBE_NSS(Firefox3NSS3Decryptor)) {
  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#if defined(OS_MACOSX)
  nss_path = nss_path.AppendASCII("firefox3_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox3_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox3_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  EXPECT_EQ(ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECKa"
                              "jtRg4qFSHBAhv9luFkXgDJA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECLW"
                              "qqiccfQHWBAie74hxnULxlw=="));
}

TEST(FirefoxImporterTest, Firefox2BookmarkParse) {
  bool result;

  // Tests charset.
  std::string charset;
  result = Firefox2Importer::ParseCharsetFromLine(
      "<META HTTP-EQUIV=\"Content-Type\" "
      "CONTENT=\"text/html; charset=UTF-8\">",
      &charset);
  EXPECT_TRUE(result);
  EXPECT_EQ("UTF-8", charset);

  // Escaped characters in name.
  string16 folder_name;
  bool is_toolbar_folder;
  base::Time folder_add_date;
  result = Firefox2Importer::ParseFolderNameFromLine(
      "<DT><H3 ADD_DATE=\"1207558707\" >&lt; &gt;"
      " &amp; &quot; &#39; \\ /</H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("< > & \" ' \\ /"), folder_name);
  EXPECT_FALSE(is_toolbar_folder);
  EXPECT_TRUE(base::Time::FromTimeT(1207558707) == folder_add_date);

  // Empty name and toolbar folder attribute.
  result = Firefox2Importer::ParseFolderNameFromLine(
      "<DT><H3 PERSONAL_TOOLBAR_FOLDER=\"true\"></H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(string16(), folder_name);
  EXPECT_TRUE(is_toolbar_folder);

  // Unicode characters in title and shortcut.
  string16 title;
  GURL url, favicon;
  string16 shortcut;
  string16 post_data;
  base::Time add_date;
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://chinese.site.cn/path?query=1#ref\" "
      "SHORTCUTURL=\"\xE4\xB8\xAD\">\xE4\xB8\xAD\xE6\x96\x87</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(L"\x4E2D\x6587", UTF16ToWide(title));
  EXPECT_EQ("http://chinese.site.cn/path?query=1#ref", url.spec());
  EXPECT_EQ(L"\x4E2D", UTF16ToWide(shortcut));
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // No shortcut, and url contains %22 ('"' character).
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22<>%22\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ("http://domain.com/?q=%22%3C%3E%22", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?g=&quot;\"\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ("http://domain.com/?g=%22", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Creation date.
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://site/\" ADD_DATE=\"1121301154\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ(GURL("http://site/"), url);
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1121301154) == add_date);

  // Post-data
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://localhost:8080/test/hello.html\" ADD_DATE=\""
      "1212447159\" LAST_VISIT=\"1212447251\" LAST_MODIFIED=\"1212447248\""
      "SHORTCUTURL=\"post\" ICON=\"data:\" POST_DATA=\"lname%3D%25s\""
      "LAST_CHARSET=\"UTF-8\" ID=\"rdf:#$weKaR3\">Test Post keyword</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("Test Post keyword"), title);
  EXPECT_EQ("http://localhost:8080/test/hello.html", url.spec());
  EXPECT_EQ(ASCIIToUTF16("post"), shortcut);
  EXPECT_EQ(ASCIIToUTF16("lname%3D%25s"), post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1212447159) == add_date);

  // Invalid case.
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(string16(), title);
  EXPECT_EQ("", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Epiphany format.
  result = Firefox2Importer::ParseMinimumBookmarkFromLine(
      "<dt><a href=\"http://www.google.com/\">Google</a></dt>",
      charset, &title, &url);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("Google"), title);
  EXPECT_EQ("http://www.google.com/", url.spec());
}

TEST(FirefoxImporterTest, Firefox2BookmarkFileImport) {
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("firefox2_importer");

  // Import all bookmarks from a file which include an empty folder entry.
  base::FilePath empty_folder_path = path.AppendASCII("empty_folder.html");
  std::set<GURL> default_urls;
  Firefox2Importer* importer = new Firefox2Importer();
  importer->AddRef();
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  importer->ImportBookmarksFile(empty_folder_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(3U, bookmarks.size());
  std::vector<ProfileWriter::BookmarkEntry>::iterator it;
  ProfileWriter::BookmarkEntry entry;
  std::vector<string16>::iterator path_it;
  if (bookmarks.size() == 3) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Empty"), entry.title);
    EXPECT_TRUE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Empty's Parent"), entry.path.front());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty"), entry.path.front());
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(0000000000), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty But Default"), entry.path.front());
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
  }

  // Import non-default bookmarks from a file.
  bookmarks.clear();
  default_urls.insert(GURL("http://www.google.com/"));
  importer->ImportBookmarksFile(empty_folder_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Empty"), entry.title);
    EXPECT_TRUE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Empty's Parent"), entry.path.front());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty"), entry.path.front());
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
  }

  // Import Epiphany bookmarks from a file
  base::FilePath epiphany_path = path.AppendASCII("epiphany.html");
  bookmarks.clear();
  default_urls.clear();
  importer->ImportBookmarksFile(epiphany_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
  }

  // Import Epiphany bookmarks from a file to bookmark bar.
  bookmarks.clear();
  default_urls.clear();
  importer->ImportBookmarksFile(epiphany_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
  }

  importer->Release();
}
