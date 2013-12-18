// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PrefRegistrySyncable;
class Profile;

namespace chrome {
class MediaGalleriesPreferences;
}

// Singleton that owns all MediaGalleriesPreferences and associates them with
// Profiles.
class MediaGalleriesPreferencesFactory : public ProfileKeyedServiceFactory {
 public:
  // Use MediaFileSystemRegistry::GetPreferences() to get
  // MediaGalleriesPreferences.
  static chrome::MediaGalleriesPreferences* GetForProfile(Profile* profile);

  static MediaGalleriesPreferencesFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaGalleriesPreferencesFactory>;

  MediaGalleriesPreferencesFactory();
  virtual ~MediaGalleriesPreferencesFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefRegistrySyncable* registry) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPreferencesFactory);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_
