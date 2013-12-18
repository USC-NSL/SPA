// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SWITCHES_H_
#define ASH_ASH_SWITCHES_H_

#include "ash/ash_export.h"

#include "build/build_config.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
ASH_EXPORT extern const char kAshAnimateFromBootSplashScreen[];
ASH_EXPORT extern const char kAshBootAnimationFunction2[];
ASH_EXPORT extern const char kAshBootAnimationFunction3[];
ASH_EXPORT extern const char kAshConstrainPointerToRoot[];
ASH_EXPORT extern const char kAshCopyHostBackgroundAtBoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDisableAutoMaximizing[];
ASH_EXPORT extern const char kAshDisableAutoWindowPlacement[];
ASH_EXPORT extern const char kAshDisableBootAnimation2[];
ASH_EXPORT extern const char kAshDisableDisplayChangeLimiter[];
ASH_EXPORT extern const char kAshDisableImmersiveFullscreen[];
ASH_EXPORT extern const char kAshDisableLauncherPerDisplay[];
ASH_EXPORT extern const char kAshDisableNewLockAnimations[];
ASH_EXPORT extern const char kAshDisableNewNetworkStatusArea[];
ASH_EXPORT extern const char kAshDisablePerAppLauncher[];
ASH_EXPORT extern const char kAshDisableUIScaling[];
ASH_EXPORT extern const char kAshDisableDisplayRotation[];
ASH_EXPORT extern const char kAshEnableAdvancedGestures[];
ASH_EXPORT extern const char kAshEnableBrightnessControl[];
ASH_EXPORT extern const char kAshEnableNewAudioHandler[];
#if defined(OS_LINUX)
ASH_EXPORT extern const char kAshEnableMemoryMonitor[];
#endif
ASH_EXPORT extern const char kAshEnableImmersiveFullscreen[];
ASH_EXPORT extern const char kAshEnableOak[];
ASH_EXPORT extern const char kAshEnableStickyEdges[];
ASH_EXPORT extern const char kAshEnableTrayDragging[];
ASH_EXPORT extern const char kAshEnableWorkspaceScrubbing[];
ASH_EXPORT extern const char kAshHostWindowBounds[];
ASH_EXPORT extern const char kAshImmersiveHideTabIndicators[];
ASH_EXPORT extern const char kAshSecondaryDisplayLayout[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAshUseFirstDisplayAsInternal[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
#if defined(OS_WIN)
ASH_EXPORT extern const char kForceAshToDesktop[];
#endif

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
