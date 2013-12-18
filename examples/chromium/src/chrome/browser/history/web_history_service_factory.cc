// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"

namespace {
// Returns true if the user is signed in and full history sync is enabled,
// and false otherwise.
bool IsHistorySyncEnabled(Profile* profile) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kHistoryDisableFullHistorySync)) {
    ProfileSyncService* sync =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
    return sync &&
        sync->GetPreferredDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  }
  return false;
}

}  // namespace

// static
WebHistoryServiceFactory* WebHistoryServiceFactory::GetInstance() {
  return Singleton<WebHistoryServiceFactory>::get();
}

// static
history::WebHistoryService* WebHistoryServiceFactory::GetForProfile(
      Profile* profile) {
  if (IsHistorySyncEnabled(profile)) {
    return static_cast<history::WebHistoryService*>(
        GetInstance()->GetServiceForProfile(profile, true));
  }
  return NULL;
}

ProfileKeyedService* WebHistoryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // Ensure that the service is not instantiated or used if the user is not
  // signed into sync, or if web history is not enabled.
  return IsHistorySyncEnabled(profile) ?
      new history::WebHistoryService(profile) : NULL;
}

WebHistoryServiceFactory::WebHistoryServiceFactory()
    : ProfileKeyedServiceFactory("WebHistoryServiceFactory",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(TokenServiceFactory::GetInstance());
  DependsOn(CookieSettings::Factory::GetInstance());
}

WebHistoryServiceFactory::~WebHistoryServiceFactory() {
}
