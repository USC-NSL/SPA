// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/environment.h"
#include "base/logging.h"
#include "chrome/browser/shell_integration_linux.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace internals {

bool CreatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const ShellIntegration::ShortcutLocations& creation_locations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::string shortcut_template;
  if (!ShellIntegrationLinux::GetDesktopShortcutTemplate(env.get(),
                                                         &shortcut_template)) {
    return false;
  }
  return ShellIntegrationLinux::CreateDesktopShortcut(
      shortcut_info, creation_locations, shortcut_template);
}

void DeletePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  ShellIntegrationLinux::DeleteDesktopShortcuts(shortcut_info.profile_path,
      shortcut_info.extension_id);
}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  scoped_ptr<base::Environment> env(base::Environment::Create());

  // Find out whether shortcuts are already installed.
  ShellIntegration::ShortcutLocations creation_locations =
      ShellIntegrationLinux::GetExistingShortcutLocations(
          env.get(), shortcut_info.profile_path, shortcut_info.extension_id);
  // Always create a hidden shortcut in applications if a visible one is not
  // being created. This allows the operating system to identify the app, but
  // not show it in the menu.
  creation_locations.hidden = true;

  CreatePlatformShortcuts(web_app_path, shortcut_info, creation_locations);
}

}  // namespace internals

}  // namespace web_app
