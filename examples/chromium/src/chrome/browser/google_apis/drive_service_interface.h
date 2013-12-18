// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_SERVICE_INTERFACE_H_

#include <string>

// TODO(kochi): Further split gdata_operations.h and include only necessary
// headers. http://crbug.com/141469
// DownloadActionCallback
#include "chrome/browser/google_apis/base_operations.h"

class Profile;

namespace net {
class IOBuffer;
}  // namespace net

namespace google_apis {

class AboutResource;
class AccountMetadata;
class AppList;
class ResourceEntry;
class ResourceList;

// Observer interface for DriveServiceInterface.
class DriveServiceObserver {
 public:
  // Triggered when the service gets ready to perform operations.
  virtual void OnReadyToPerformOperations() {}

  // Called when the refresh token was found to be invalid.
  virtual void OnRefreshTokenInvalid() {}

 protected:
  virtual ~DriveServiceObserver() {}
};

// Callback used for GetResourceList().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ResourceList> resource_list)>
    GetResourceListCallback;

// Callback used for GetResourceEntry().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ResourceEntry> entry)>
    GetResourceEntryCallback;

// Callback used for GetAccountMetadata().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AccountMetadata> account_metadata)>
    GetAccountMetadataCallback;

// Callback used for GetAboutResource().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AboutResource> about_resource)>
    GetAboutResourceCallback;

// Callback used for GetApplicationInfo().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AppList> app_list)>
    GetAppListCallback;

// Callback used for ResumeUpload() and GetUploadStatus().
typedef base::Callback<void(
    const UploadRangeResponse& response,
    scoped_ptr<ResourceEntry> new_entry)> UploadRangeCallback;

// Callback used for AuthorizeApp(). |open_url| is used to open the target
// file with the authorized app.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& open_url)>
    AuthorizeAppCallback;

// This defines an interface for sharing by DriveService and MockDriveService
// so that we can do testing of clients of DriveService.
//
// All functions must be called on UI thread. DriveService is built on top of
// URLFetcher that runs on UI thread.
class DriveServiceInterface {
 public:
  virtual ~DriveServiceInterface() {}

  // Common service:

  // Initializes the documents service tied with |profile|.
  virtual void Initialize(Profile* profile) = 0;

  // Adds an observer.
  virtual void AddObserver(DriveServiceObserver* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(DriveServiceObserver* observer) = 0;

  // True if ready to start operations.
  virtual bool CanStartOperation() const = 0;

  // Cancels all in-flight operations.
  virtual void CancelAll() = 0;

  // Cancels ongoing operation for a given virtual |file_path|. Returns true if
  // the operation was found and canceled.
  virtual bool CancelForFilePath(const base::FilePath& file_path) = 0;

  // Authentication service:

  // True if OAuth2 access token is retrieved and believed to be fresh.
  virtual bool HasAccessToken() const = 0;

  // True if OAuth2 refresh token is present.
  virtual bool HasRefreshToken() const = 0;

  // Clears OAuth2 access token.
  virtual void ClearAccessToken() = 0;

  // Clears OAuth2 refresh token.
  virtual void ClearRefreshToken() = 0;

  // Document access:

  // Returns the resource id for the root directory.
  virtual std::string GetRootResourceId() const = 0;

  // Fetches a resource list of the account. |callback| will be called upon
  // completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // ContinueGetResourceList.
  //
  // |callback| must not be null.
  virtual void GetAllResourceList(
      const GetResourceListCallback& callback) = 0;

  // Fetches a resource list in the directory with |directory_resource_id|.
  // |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // ContinueGetResourceList.
  //
  // |directory_resource_id| must not be empty.
  // |callback| must not be null.
  virtual void GetResourceListInDirectory(
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) = 0;

  // Searches the resources for the |search_query| from all the user's
  // resources. |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // ContinueGetResourceList.
  //
  // |search_query| must not be empty.
  // |callback| must not be null.
  virtual void Search(
      const std::string& search_query,
      const GetResourceListCallback& callback) = 0;

  // Searches the resources with the |title|.
  // |directory_resource_id| is an optional prameter. If it is empty,
  // the search target is all the existing resources. Otherwise, it is
  // the resources directly under the directory with |directory_resource_id|.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // ContinueGetResourceList.
  //
  // |title| must not be empty, and |callback| must not be null.
  virtual void SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) = 0;

  // Fetches change list since |start_changestamp|. |callback| will be
  // called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // ContinueGetResourceList.
  //
  // |callback| must not be null.
  virtual void GetChangeList(
      int64 start_changestamp,
      const GetResourceListCallback& callback) = 0;

  // Operations returning GetResourceList may be paged. In such a case,
  // a URL to fetch remaining result is returned. The URL can be used for this
  // method. |callback| will be called upon completion.
  //
  // |override_url| must not be empty.
  // |callback| must not be null.
  virtual void ContinueGetResourceList(
      const GURL& override_url,
      const GetResourceListCallback& callback) = 0;

  // Fetches single entry metadata from server. The entry's resource id equals
  // |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void GetResourceEntry(const std::string& resource_id,
                                const GetResourceEntryCallback& callback) = 0;

  // Gets the account metadata from the server using the default account
  // metadata URL. Upon completion, invokes |callback| with results on the
  // calling thread.
  // |callback| must not be null.
  virtual void GetAccountMetadata(
      const GetAccountMetadataCallback& callback) = 0;

  // Gets the about resource information from the server.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void GetAboutResource(const GetAboutResourceCallback& callback) = 0;

  // Gets the application information from the server.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void GetAppList(const GetAppListCallback& callback) = 0;

  // Deletes a resource identified by its |resource_id|.
  // If |etag| is not empty and did not match, the deletion fails with
  // HTTP_PRECONDITION error.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void DeleteResource(const std::string& resource_id,
                              const std::string& etag,
                              const EntryActionCallback& callback) = 0;

  // Makes a copy of a hosted document identified by its |resource_id|.
  // The copy is named as the UTF-8 encoded |new_name| and is not added to any
  // collection. Use AddResourceToDirectory() to add the copy to a collection
  // when needed. Upon completion, invokes |callback| with results on the
  // calling thread.
  // |callback| must not be null.
  virtual void CopyHostedDocument(
      const std::string& resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback) = 0;

  // Renames a document or collection identified by its |resource_id|
  // to the UTF-8 encoded |new_name|. Upon completion,
  // invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void RenameResource(const std::string& resource_id,
                              const std::string& new_name,
                              const EntryActionCallback& callback) = 0;

  // Adds a resource (document, file, or collection) identified by its
  // |resource_id| to a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void AddResourceToDirectory(const std::string& parent_resource_id,
                                      const std::string& resource_id,
                                      const EntryActionCallback& callback) = 0;

  // Removes a resource (document, file, collection) identified by its
  // |resource_id| from a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual void RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback) = 0;

  // Adds new collection with |directory_name| under parent directory
  // identified with |parent_resource_id|. |parent_resource_id| can be the
  // value returned by GetRootResourceId to represent the root directory.
  // Upon completion, invokes |callback| and passes newly created entry on
  // the calling thread.
  // This function cannot be named as "CreateDirectory" as it conflicts with
  // a macro on Windows.
  // |callback| must not be null.
  virtual void AddNewDirectory(const std::string& parent_resource_id,
                               const std::string& directory_name,
                               const GetResourceEntryCallback& callback) = 0;

  // Downloads a file from |download_url|. The downloaded file will
  // be stored at |local_cache_path| location. Upon completion, invokes
  // |download_action_callback| with results on the calling thread.
  // If |get_content_callback| is not empty,
  // URLFetcherDelegate::OnURLFetchDownloadData will be called, which will in
  // turn invoke |get_content_callback| on the calling thread.
  // If |progress_callback| is not empty, it is invoked periodically when
  // the download made some progress.
  //
  // |download_action_callback| must not be null.
  // |get_content_callback| and |progress_callback| may be null.
  virtual void DownloadFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_cache_path,
      const GURL& download_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback) = 0;

  // Initiates uploading of a new document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual void InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) = 0;

  // Initiates uploading of an existing document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) = 0;

  // Resumes uploading of a document/file on the calling thread.
  // |callback| must not be null. |progress_callback| may be null.
  virtual void ResumeUpload(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) = 0;

  // Gets the current status of the uploading to |upload_url| from the server.
  // |upload_mode|, |drive_file_path| and |content_length| should be set to
  // the same value which is used for ResumeUpload.
  // |callback| must not be null.
  virtual void GetUploadStatus(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback) = 0;

  // Authorizes a Drive app with the id |app_id| to open the given file.
  // Upon completion, invokes |callback| with the link to open the file with
  // the provided app. |callback| must not be null.
  virtual void AuthorizeApp(const std::string& resource_id,
                            const std::string& app_id,
                            const AuthorizeAppCallback& callback) = 0;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_SERVICE_INTERFACE_H_
