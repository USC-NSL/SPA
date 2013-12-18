// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_

#include <map>
#include <set>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"

namespace net {
class BoundNetLog;
}

namespace content {
class DownloadFileFactory;
class DownloadItemFactory;
class DownloadItemImpl;
class DownloadRequestHandleInterface;

class CONTENT_EXPORT DownloadManagerImpl : public DownloadManager,
                                           private DownloadItemImplDelegate {
 public:
  // Caller guarantees that |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  DownloadManagerImpl(net::NetLog* net_log, BrowserContext* browser_context);

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual DownloadItemImpl* CreateSavePackageDownloadItem(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      scoped_ptr<DownloadRequestHandleInterface> request_handle,
      DownloadItem::Observer* observer);

  // Notifies DownloadManager about a successful completion of |download_item|.
  void OnSavePackageSuccessfullyFinished(DownloadItem* download_item);

  // DownloadManager functions.
  virtual void SetDelegate(DownloadManagerDelegate* delegate) OVERRIDE;
  virtual DownloadManagerDelegate* GetDelegate() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void GetAllDownloads(DownloadVector* result) OVERRIDE;
  virtual DownloadItem* StartDownload(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<ByteStreamReader> stream) OVERRIDE;
  virtual void CancelDownload(int32 download_id) OVERRIDE;
  virtual int RemoveDownloadsBetween(base::Time remove_begin,
                                     base::Time remove_end) OVERRIDE;
  virtual int RemoveDownloads(base::Time remove_begin) OVERRIDE;
  virtual int RemoveAllDownloads() OVERRIDE;
  virtual void DownloadUrl(scoped_ptr<DownloadUrlParameters> params) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual content::DownloadItem* CreateDownloadItem(
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const base::Time& start_time,
      const base::Time& end_time,
      int64 received_bytes,
      int64 total_bytes,
      content::DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened) OVERRIDE;
  virtual int InProgressCount() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void CheckForHistoryFilesRemoval() OVERRIDE;
  virtual DownloadItem* GetDownload(int id) OVERRIDE;

  // For testing; specifically, accessed from TestFileErrorInjector.
  void SetDownloadItemFactoryForTesting(
      scoped_ptr<DownloadItemFactory> item_factory);
  void SetDownloadFileFactoryForTesting(
      scoped_ptr<DownloadFileFactory> file_factory);
  virtual DownloadFileFactory* GetDownloadFileFactoryForTesting();

 private:
  typedef std::set<DownloadItem*> DownloadSet;
  typedef base::hash_map<int32, DownloadItemImpl*> DownloadMap;
  typedef std::vector<DownloadItemImpl*> DownloadItemImplVector;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  friend class base::RefCountedThreadSafe<DownloadManagerImpl>;

  virtual ~DownloadManagerImpl();

  // Create a new active item based on the info.  Separate from
  // StartDownload() for testing.
  void CreateActiveItem(DownloadId id, const DownloadCreateInfo& info);

  // Get next download id.
  DownloadId GetNextId();

  // Called with the result of DownloadManagerDelegate::CheckForFileExistence.
  // Updates the state of the file and then notifies this update to the file's
  // observer.
  void OnFileExistenceChecked(int32 download_id, bool result);

  // Overridden from DownloadItemImplDelegate
  // (Note that |GetBrowserContext| are present in both interfaces.)
  virtual void DetermineDownloadTarget(
      DownloadItemImpl* item, const DownloadTargetCallback& callback) OVERRIDE;
  virtual bool ShouldCompleteDownload(
      DownloadItemImpl* item, const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(
      const base::FilePath& path) OVERRIDE;
  virtual bool ShouldOpenDownload(
      DownloadItemImpl* item,
      const ShouldOpenDownloadCallback& callback) OVERRIDE;
  virtual void CheckForFileRemoval(DownloadItemImpl* download_item) OVERRIDE;
  virtual void ResumeInterruptedDownload(
      scoped_ptr<content::DownloadUrlParameters> params,
      content::DownloadId id) OVERRIDE;
  virtual void OpenDownload(DownloadItemImpl* download) OVERRIDE;
  virtual void ShowDownloadInShell(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadRemoved(DownloadItemImpl* download) OVERRIDE;

  // Factory for creation of downloads items.
  scoped_ptr<DownloadItemFactory> item_factory_;

  // Factory for the creation of download files.
  scoped_ptr<DownloadFileFactory> file_factory_;

  // |downloads_| is the owning set for all downloads known to the
  // DownloadManager.  This includes downloads started by the user in
  // this session, downloads initialized from the history system, and
  // "save page as" downloads.
  DownloadMap downloads_;

  int history_size_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active browser context.
  BrowserContext* browser_context_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  DownloadManagerDelegate* delegate_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
