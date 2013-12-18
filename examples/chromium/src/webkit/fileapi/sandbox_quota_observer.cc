// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_quota_observer.h"

#include "base/sequenced_task_runner.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

SandboxQuotaObserver::SandboxQuotaObserver(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* update_notify_runner,
    ObfuscatedFileUtil* sandbox_file_util,
    FileSystemUsageCache* file_system_usage_cache)
    : quota_manager_proxy_(quota_manager_proxy),
      update_notify_runner_(update_notify_runner),
      sandbox_file_util_(sandbox_file_util),
      file_system_usage_cache_(file_system_usage_cache),
      running_delayed_cache_update_(false),
      weak_factory_(this) {}

SandboxQuotaObserver::~SandboxQuotaObserver() {}

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::IsSandboxType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  file_system_usage_cache_->IncrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url,
                                    int64 delta) {
  DCHECK(SandboxMountPointProvider::IsSandboxType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());

  if (quota_manager_proxy_) {
    quota_manager_proxy_->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()),
        delta);
  }

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  pending_update_notification_[usage_file_path] += delta;
  if (!running_delayed_cache_update_) {
    update_notify_runner_->PostTask(FROM_HERE, base::Bind(
        &SandboxQuotaObserver::ApplyPendingUsageUpdate,
        weak_factory_.GetWeakPtr()));
    running_delayed_cache_update_ = true;
  }
}

void SandboxQuotaObserver::OnEndUpdate(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::IsSandboxType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());

  base::FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;

  PendingUpdateNotificationMap::iterator found =
      pending_update_notification_.find(usage_file_path);
  if (found != pending_update_notification_.end()) {
    UpdateUsageCacheFile(found->first, found->second);
    pending_update_notification_.erase(found);
  }

  file_system_usage_cache_->DecrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnAccess(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::IsSandboxType(url.type()));
  if (quota_manager_proxy_) {
    quota_manager_proxy_->NotifyStorageAccessed(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()));
  }
}

void SandboxQuotaObserver::SetUsageCacheEnabled(
    const GURL& origin,
    FileSystemType type,
    bool enabled) {
  if (quota_manager_proxy_) {
    quota_manager_proxy_->SetUsageCacheEnabled(
        quota::QuotaClient::kFileSystem,
        origin, FileSystemTypeToQuotaStorageType(type),
        enabled);
  }
}

base::FilePath SandboxQuotaObserver::GetUsageCachePath(
    const FileSystemURL& url) {
  DCHECK(sandbox_file_util_);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path =
      SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
          sandbox_file_util_, url.origin(), url.type(), &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(WARNING) << "Could not get usage cache path for: "
                 << url.DebugString();
    return base::FilePath();
  }
  return path;
}

void SandboxQuotaObserver::ApplyPendingUsageUpdate() {
  for (PendingUpdateNotificationMap::iterator itr =
           pending_update_notification_.begin();
       itr != pending_update_notification_.end();
       ++itr) {
    UpdateUsageCacheFile(itr->first, itr->second);
  }
  pending_update_notification_.clear();
  running_delayed_cache_update_ = false;
}

void SandboxQuotaObserver::UpdateUsageCacheFile(
    const base::FilePath& usage_file_path,
    int64 delta) {
  DCHECK(!usage_file_path.empty());
  if (!usage_file_path.empty() && delta != 0)
    file_system_usage_cache_->AtomicUpdateUsageByDelta(usage_file_path, delta);
}

}  // namespace fileapi
