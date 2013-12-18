// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ProfileSyncService;

class ProfileSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ProfileSyncService* GetForProfile(Profile* profile);
  static bool HasProfileSyncService(Profile* profile);

  static ProfileSyncServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProfileSyncServiceFactory>;

  ProfileSyncServiceFactory();
  virtual ~ProfileSyncServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
