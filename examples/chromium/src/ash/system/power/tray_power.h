// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_H_

#include "ash/system/power/power_status_observer.h"
#include "ash/system/tray/system_tray_item.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
}

namespace ash {
namespace internal {

namespace tray {
class PowerNotificationView;
class PowerTrayView;
}

enum IconSet {
  ICON_LIGHT,
  ICON_DARK
};

class TrayPower : public SystemTrayItem,
                  public PowerStatusObserver {
 public:
  explicit TrayPower(SystemTray* system_tray);
  virtual ~TrayPower();

  // Gets the icon index in the battery icon array image based on
  // |supply_status|.  If |supply_status| is uncertain about the power state,
  // returns -1.
  static int GetBatteryImageIndex(const PowerSupplyStatus& supply_status);

  // Looks up the actual icon in the icon array image for |image_index|.
  static gfx::ImageSkia GetBatteryImage(int image_index, IconSet icon_set);

  // Gets the battery accessible string for |supply_status|.
  static base::string16 GetAccessibleNameString(
      const PowerSupplyStatus& supply_status);

  // Gets rounded battery percentage for |battery_percentage|.
  static int GetRoundedBatteryPercentage(double battery_percentage);

 private:
  enum NotificationState {
    NOTIFICATION_NONE,
    NOTIFICATION_LOW_POWER,
    NOTIFICATION_CRITICAL
  };

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // Overridden from PowerStatusObserver.
  virtual void OnPowerStatusChanged(const PowerSupplyStatus& status) OVERRIDE;

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState(const PowerSupplyStatus& status);

  tray::PowerTrayView* power_tray_;
  tray::PowerNotificationView* notification_view_;
  NotificationState notification_state_;

  DISALLOW_COPY_AND_ASSIGN(TrayPower);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_H_
