// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "content/browser/accessibility/browser_accessibility_mac.h"

#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_delegate_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

namespace content {

// Static.
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityMac();
}

BrowserAccessibilityMac::BrowserAccessibilityMac()
    : browser_accessibility_cocoa_(NULL) {
}

void BrowserAccessibilityMac::PreInitialize() {
  BrowserAccessibility::PreInitialize();

  if (browser_accessibility_cocoa_)
    return;

  // We take ownership of the cocoa obj here.
  BrowserAccessibilityManagerMac* manager =
      static_cast<BrowserAccessibilityManagerMac*>(manager_);
  browser_accessibility_cocoa_ = [[BrowserAccessibilityCocoa alloc]
      initWithObject:this
      delegate:
          (id<BrowserAccessibilityDelegateCocoa>)manager->parent_view()];
}

void BrowserAccessibilityMac::NativeReleaseReference() {
  if (browser_accessibility_cocoa_) {
    BrowserAccessibilityCocoa* temp = browser_accessibility_cocoa_;
    browser_accessibility_cocoa_ = nil;
    // Relinquish ownership of the cocoa obj.
    [temp release];
    // At this point, other processes may have a reference to
    // the cocoa object. When the retain count hits zero, it will
    // destroy us in dealloc.
    // For that reason, do *not* make any more calls here after
    // as we might have been deleted.
  }
}

bool BrowserAccessibilityMac::IsNative() const {
  return true;
}

void BrowserAccessibilityMac::DetachTree(
    std::vector<BrowserAccessibility*>* nodes) {
  [browser_accessibility_cocoa_ childrenChanged];
  BrowserAccessibility::DetachTree(nodes);
}

void BrowserAccessibilityMac::SwapChildren(
    std::vector<BrowserAccessibility*>& children) {
  [browser_accessibility_cocoa_ childrenChanged];
  BrowserAccessibility::SwapChildren(children);
}

BrowserAccessibilityCocoa* BrowserAccessibility::ToBrowserAccessibilityCocoa() {
  return static_cast<BrowserAccessibilityMac*>(this)->
      native_view();
}

}  // namespace content
