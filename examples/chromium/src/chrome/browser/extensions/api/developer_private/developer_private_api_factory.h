// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class DeveloperPrivateAPI;

// This is a singleton class which holds profileKeyed references to
// DeveloperPrivateAPI class.
class DeveloperPrivateAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static DeveloperPrivateAPI* GetForProfile(Profile* profile);

  static DeveloperPrivateAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<DeveloperPrivateAPIFactory>;

  DeveloperPrivateAPIFactory();
  virtual ~DeveloperPrivateAPIFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_FACTORY_H_
