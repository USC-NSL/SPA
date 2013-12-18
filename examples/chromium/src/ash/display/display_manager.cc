// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/aura/remote_root_window_host_win.h"
#endif

DECLARE_WINDOW_PROPERTY_TYPE(int64);

namespace ash {
namespace internal {
typedef std::vector<gfx::Display> DisplayList;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

// List of value UI Scale values. Scales for 2x are equivalent to 640,
// 800, 1024, 1280, 1440, 1600 and 1920 pixel width respectively on
// 2560 pixel width 2x density display. Please see crbug.com/233375
// for the full list of resolutions.
const float kUIScalesFor2x[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f, 1.25f, 1.5f};
const float kUIScalesFor1280[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f };
const float kUIScalesFor1366[] = {0.5f, 0.6f, 0.75f, 1.0f, 1.125f };

struct DisplaySortFunctor {
  bool operator()(const gfx::Display& a, const gfx::Display& b) {
    return a.id() < b.id();
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const DisplayInfo& a, const DisplayInfo& b) {
    return a.id() < b.id();
  }
};

struct ScaleComparator {
  ScaleComparator(float s) : scale(s) {}

  bool operator()(float s) const {
    const float kEpsilon = 0.0001f;
    return std::abs(scale - s) < kEpsilon;
  }
  float scale;
};

std::vector<float> GetScalesForDisplay(const DisplayInfo& info) {
  std::vector<float> ret;
  if (info.device_scale_factor() == 2.0f) {
    ret.assign(kUIScalesFor2x, kUIScalesFor2x + arraysize(kUIScalesFor2x));
    return ret;
  }
  switch (info.bounds_in_pixel().width()) {
    case 1280:
      ret.assign(kUIScalesFor1280,
                 kUIScalesFor1280 + arraysize(kUIScalesFor1280));
      break;
    case 1366:
      ret.assign(kUIScalesFor1366,
                 kUIScalesFor1366 + arraysize(kUIScalesFor1366));
      break;
    default:
      ret.assign(kUIScalesFor1280,
                 kUIScalesFor1280 + arraysize(kUIScalesFor1280));
#if defined(OS_CHROMEOS)
      if (base::chromeos::IsRunningOnChromeOS())
        NOTREACHED() << "Unknown resolution:" << info.ToString();
#endif
  }
  return ret;
}

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

}  // namespace

using aura::RootWindow;
using aura::Window;
using std::string;
using std::vector;

DEFINE_WINDOW_PROPERTY_KEY(int64, kDisplayIdKey,
                           gfx::Display::kInvalidDisplayID);

DisplayManager::DisplayManager()
    : first_display_id_(gfx::Display::kInvalidDisplayID),
      mirrored_display_id_(gfx::Display::kInvalidDisplayID),
      num_connected_displays_(0),
      force_bounds_changed_(false),
      change_display_upon_host_resize_(false) {
#if defined(OS_CHROMEOS)
  change_display_upon_host_resize_ = !base::chromeos::IsRunningOnChromeOS();
#endif
  Init();
}

DisplayManager::~DisplayManager() {
}

// static
void DisplayManager::CycleDisplay() {
  Shell::GetInstance()->display_manager()->CycleDisplayImpl();
}

// static
void DisplayManager::ToggleDisplayScaleFactor() {
  Shell::GetInstance()->display_manager()->ScaleDisplayImpl();
}

// static
float DisplayManager::GetNextUIScale(const DisplayInfo& info, bool up) {
  float scale = info.ui_scale();
  std::vector<float> scales = GetScalesForDisplay(info);
  for (size_t i = 0; i < scales.size(); ++i) {
    if (ScaleComparator(scales[i])(scale)) {
      if (up && i != scales.size() - 1)
        return scales[i + 1];
      if (!up && i != 0)
        return scales[i - 1];
      return scales[i];
    }
  }
  // Fallback to 1.0f if the |scale| wasn't in the list.
  return 1.0f;
}

bool DisplayManager::IsActiveDisplay(const gfx::Display& display) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == display.id())
      return true;
  }
  return false;
}

bool DisplayManager::HasInternalDisplay() const {
  return gfx::Display::InternalDisplayId() != gfx::Display::kInvalidDisplayID;
}

bool DisplayManager::IsInternalDisplayId(int64 id) const {
  return gfx::Display::InternalDisplayId() == id;
}

bool DisplayManager::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const RootWindow* root = window->GetRootWindow();
  gfx::Display& display = FindDisplayForRootWindow(root);
  gfx::Rect old_work_area = display.work_area();
  display.UpdateWorkAreaFromInsets(insets);
  return old_work_area != display.work_area();
}

const gfx::Display& DisplayManager::GetDisplayForId(int64 id) const {
  return const_cast<DisplayManager*>(this)->FindDisplayForId(id);
}

const gfx::Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    if (display.bounds().Contains(point_in_screen))
      return display;
  }
  return GetInvalidDisplay();
}

void DisplayManager::SetOverscanInsets(int64 display_id,
                                       const gfx::Insets& insets_in_dip) {
  // TODO(oshima): insets has to be rotated according to the
  // the current display rotation.
  display_info_[display_id].SetOverscanInsets(true, insets_in_dip);
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::ClearCustomOverscanInsets(int64 display_id) {
  display_info_[display_id].clear_has_custom_overscan_insets();
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::SetDisplayRotation(int64 display_id,
                                        gfx::Display::Rotation rotation) {
  if (!IsDisplayRotationEnabled())
    return;
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (info.rotation() == rotation)
        return;
      info.set_rotation(rotation);
    }
    display_info_list.push_back(info);
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::SetDisplayUIScale(int64 display_id,
                                       float ui_scale) {
  if (!IsDisplayUIScalingEnabled() ||
      gfx::Display::InternalDisplayId() != display_id) {
    return;
  }

  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (info.ui_scale() == ui_scale)
        return;
      std::vector<float> scales = GetScalesForDisplay(info);
      ScaleComparator comparator(ui_scale);
      if (std::find_if(scales.begin(), scales.end(), comparator) ==
          scales.end()) {
        return;
      }
      info.set_ui_scale(ui_scale);
    }
    display_info_list.push_back(info);
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::RegisterDisplayProperty(
    int64 display_id,
    gfx::Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets) {
  if (display_info_.find(display_id) == display_info_.end()) {
    display_info_[display_id] =
        DisplayInfo(display_id, std::string(""), false);
  }

  display_info_[display_id].set_rotation(rotation);
  // Just in case the preference file was corrupted.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(true, *overscan_insets);
}

bool DisplayManager::IsDisplayRotationEnabled() const {
  static bool enabled = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAshDisableDisplayRotation);
  return enabled;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  static bool enabled = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAshDisableUIScaling);
  if (!enabled)
    return false;
  return GetDisplayIdForUIScaling() != gfx::Display::kInvalidDisplayID;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ?
      it->second.overscan_insets_in_dip() : gfx::Insets();
}

void DisplayManager::OnNativeDisplaysChanged(
    const std::vector<DisplayInfo>& updated_displays) {
  if (updated_displays.empty()) {
    // Don't update the displays when all displays are disconnected.
    // This happens when:
    // - the device is idle and powerd requested to turn off all displays.
    // - the device is suspended. (kernel turns off all displays)
    // - the internal display's brightness is set to 0 and no external
    //   display is connected.
    // - the internal display's brightness is 0 and external display is
    //   disconnected.
    // The display will be updated when one of displays is turned on, and the
    // display list will be updated correctly.
    return;
  }
  first_display_id_ = updated_displays[0].id();
  std::set<int> y_coords;
  if (updated_displays.size() == 1) {
    VLOG(1) << "OnNativeDisplaysChanged(1):" << updated_displays[0].ToString();
  } else {
    VLOG(1) << "OnNativeDisplaysChanged(" << updated_displays.size()
            << ") [0]=" << updated_displays[0].ToString()
            << ", [1]=" << updated_displays[1].ToString();
  }

  bool internal_display_connected = false;
  num_connected_displays_ = updated_displays.size();
  mirrored_display_id_ = gfx::Display::kInvalidDisplayID;
  DisplayInfoList new_display_info_list;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end();
       ++iter) {
    if (!internal_display_connected)
      internal_display_connected = IsInternalDisplayId(iter->id());
    // Mirrored monitors have the same y coordinates.
    int y = iter->bounds_in_pixel().y();
    if (y_coords.find(y) != y_coords.end()) {
      InsertAndUpdateDisplayInfo(*iter);
      mirrored_display_id_ = iter->id();
    } else {
      y_coords.insert(y);
      new_display_info_list.push_back(*iter);
    }
  }
  if (HasInternalDisplay() &&
      !internal_display_connected &&
      display_info_.find(gfx::Display::InternalDisplayId()) ==
      display_info_.end()) {
    DisplayInfo internal_display_info(
        gfx::Display::InternalDisplayId(),
        l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME),
        false  /*Internal display must not have overscan */);
    internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
    display_info_[gfx::Display::InternalDisplayId()] = internal_display_info;
  }
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::UpdateDisplays(
    const std::vector<DisplayInfo>& updated_display_info_list) {
  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(displays_.begin(), displays_.end(), DisplaySortFunctor());
  std::sort(new_display_info_list.begin(),
            new_display_info_list.end(),
            DisplayInfoSortFunctor());
  DisplayList removed_displays;
  std::vector<size_t> changed_display_indices;
  std::vector<size_t> added_display_indices;

  DisplayList::iterator curr_iter = displays_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  DisplayList new_displays;
  bool update_mouse_location = false;

  while (curr_iter != displays_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == displays_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
      update_mouse_location = true;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const gfx::Display& current_display = *curr_iter;
      // Copy the info because |CreateDisplayFromInfo| updates the instance.
      const DisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      gfx::Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const DisplayInfo& new_display_info = GetDisplayInfo(new_display.id());

      bool host_window_bounds_changed =
          current_display_info.bounds_in_pixel() !=
          new_display_info.bounds_in_pixel();

      // TODO(oshima): Rotating square dislay doesn't work as the size
      // won't change. This doesn't cause a problem now as there is no
      // such display. This will be fixed by comparing the rotation as
      // well when the rotation variable is added to gfx::Display.
      if (force_bounds_changed_ ||
          host_window_bounds_changed ||
          (current_display.device_scale_factor() !=
           new_display.device_scale_factor()) ||
          (current_display_info.size_in_pixel() !=
           new_display.GetSizeInPixel())) {

        // Don't update mouse location if the display size has
        // changed due to rotation or zooming.
        if (host_window_bounds_changed)
          update_mouse_location = true;

        changed_display_indices.push_back(new_displays.size());
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (curr_iter->id() < new_info_iter->id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
      update_mouse_location = true;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }

  // Do not update |displays_| if there's nothing to be updated. Without this,
  // it will not update the display layout, which causes the bug
  // http://crbug.com/155948.
  if (changed_display_indices.empty() && added_display_indices.empty() &&
      removed_displays.empty()) {
    return;
  }

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  gfx::Point mouse_location_in_native;
  display_controller->NotifyDisplayConfigurationChanging();
  mouse_location_in_native = display_controller->GetNativeMouseCursorLocation();

  displays_ = new_displays;

  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  displays_.insert(displays_.end(), removed_displays.begin(),
                   removed_displays.end());

  for (DisplayList::const_reverse_iterator iter = removed_displays.rbegin();
       iter != removed_displays.rend(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayRemoved(displays_.back());
    displays_.pop_back();
  }
  for (std::vector<size_t>::iterator iter = added_display_indices.begin();
       iter != added_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayAdded(displays_[*iter]);
  }
  for (std::vector<size_t>::iterator iter = changed_display_indices.begin();
       iter != changed_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyBoundsChanged(displays_[*iter]);
  }
  display_controller->NotifyDisplayConfigurationChanged();
  if (update_mouse_location)
    display_controller->EnsurePointerInDisplays();
  else
    display_controller->UpdateMouseCursor(mouse_location_in_native);

#if defined(USE_X11) && defined(OS_CHROMEOS)
  if (!changed_display_indices.empty() && base::chromeos::IsRunningOnChromeOS())
    ui::ClearX11DefaultRootWindow();
#endif
}

gfx::Display* DisplayManager::GetDisplayAt(size_t index) {
  return index < displays_.size() ? &displays_[index] : NULL;
}

const gfx::Display* DisplayManager::GetPrimaryDisplayCandidate() const {
  const gfx::Display* primary_candidate = &displays_[0];
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    // On ChromeOS device, root windows are stacked vertically, and
    // default primary is the one on top.
    int count = GetNumDisplays();
    int y = GetDisplayInfo(primary_candidate->id()).bounds_in_pixel().y();
    for (int i = 1; i < count; ++i) {
      const gfx::Display* display = &displays_[i];
      const DisplayInfo& display_info = GetDisplayInfo(display->id());
      if (display->IsInternal()) {
        primary_candidate = display;
        break;
      } else if (display_info.bounds_in_pixel().y() < y) {
        primary_candidate = display;
        y = display_info.bounds_in_pixel().y();
      }
    }
  }
#endif
  return primary_candidate;
}

size_t DisplayManager::GetNumDisplays() const {
  return displays_.size();
}

bool DisplayManager::IsMirrored() const {
  return mirrored_display_id_ != gfx::Display::kInvalidDisplayID;
}

const gfx::Display& DisplayManager::GetDisplayNearestWindow(
    const Window* window) const {
  if (!window)
    return DisplayController::GetPrimaryDisplay();
  const RootWindow* root = window->GetRootWindow();
  DisplayManager* manager = const_cast<DisplayManager*>(this);
  return root ?
      manager->FindDisplayForRootWindow(root) :
      DisplayController::GetPrimaryDisplay();
}

const gfx::Display& DisplayManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  // Fallback to the primary display if there is no root display containing
  // the |point|.
  const gfx::Display& display = FindDisplayContainingPoint(point);
  return display.is_valid() ? display : DisplayController::GetPrimaryDisplay();
}

const gfx::Display& DisplayManager::GetDisplayMatching(
    const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return GetDisplayNearestPoint(rect.origin());

  int max = 0;
  const gfx::Display* matching = 0;
  for (std::vector<gfx::Display>::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    gfx::Rect intersect = gfx::IntersectRects(display.bounds(), rect);
    int area = intersect.width() * intersect.height();
    if (area > max) {
      max = area;
      matching = &(*iter);
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : DisplayController::GetPrimaryDisplay();
}

const DisplayInfo& DisplayManager::GetDisplayInfo(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end());
  return iter->second;
}

std::string DisplayManager::GetDisplayNameForId(int64 id) {
  if (id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64, DisplayInfo>::const_iterator iter = display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

void DisplayManager::OnRootWindowResized(const aura::RootWindow* root,
                                         const gfx::Size& old_size) {
  if (change_display_upon_host_resize_) {
    gfx::Display& display = FindDisplayForRootWindow(root);
    gfx::Size old_display_size_in_pixel = display.GetSizeInPixel();
    display_info_[display.id()].SetBounds(
        gfx::Rect(root->GetHostOrigin(), root->GetHostSize()));
    const gfx::Size& new_root_size = root->bounds().size();
    if (old_size != new_root_size) {
      display.SetSize(display_info_[display.id()].size_in_pixel());
      Shell::GetInstance()->screen()->NotifyBoundsChanged(display);
    }
  }
}

int64 DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective only on internal display.
  int64 display_id = gfx::Display::InternalDisplayId();
#if defined(OS_WIN)
  display_id = first_display_id();
#endif
  return display_id;
}

void DisplayManager::Init() {
  // TODO(oshima): Move this logic to DisplayChangeObserver.
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAshHostWindowBounds);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    AddDisplayFromSpec(*iter);
  }
  if (displays_.empty())
    AddDisplayFromSpec(std::string() /* default */);
  first_display_id_ = displays_[0].id();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUseFirstDisplayAsInternal))
    gfx::Display::SetInternalDisplayId(first_display_id_);
  num_connected_displays_ = displays_.size();
}

void DisplayManager::CycleDisplayImpl() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  new_display_info_list.push_back(
      GetDisplayInfo(DisplayController::GetPrimaryDisplay().id()));
  // Add if there is only one display.
  if (displays_.size() == 1) {
    // Layout the 2nd display below the primary as with the real device.
    aura::RootWindow* primary = Shell::GetPrimaryRootWindow();
    gfx::Rect host_bounds =
        gfx::Rect(primary->GetHostOrigin(),  primary->GetHostSize());
    new_display_info_list.push_back(DisplayInfo::CreateFromSpec(
        base::StringPrintf(
            "%d+%d-500x400", host_bounds.x(), host_bounds.bottom())));
  }
  num_connected_displays_ = new_display_info_list.size();
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::ScaleDisplayImpl() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  UpdateDisplays(new_display_info_list);
}

gfx::Display& DisplayManager::FindDisplayForRootWindow(
    const aura::RootWindow* root_window) {
  int64 id = root_window->GetProperty(kDisplayIdKey);
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  gfx::Display& display = FindDisplayForId(id);
  DCHECK(display.is_valid());
  return display;
}

gfx::Display& DisplayManager::FindDisplayForId(int64 id) {
  for (DisplayList::iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == id)
      return *iter;
  }
  DLOG(WARNING) << "Could not find display:" << id;
  return GetInvalidDisplay();
}

void DisplayManager::AddDisplayFromSpec(const std::string& spec) {
  DisplayInfo display_info = DisplayInfo::CreateFromSpec(spec);
  InsertAndUpdateDisplayInfo(display_info);
  gfx::Display display = CreateDisplayFromDisplayInfoById(display_info.id());
  displays_.push_back(display);
}

void DisplayManager::InsertAndUpdateDisplayInfo(const DisplayInfo& new_info) {
  std::map<int64, DisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end())
    info->second.Copy(new_info);
  else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
  }
  display_info_[new_info.id()].UpdateDisplaySize();
}

gfx::Display DisplayManager::CreateDisplayFromDisplayInfoById(int64 id) {
  DCHECK(display_info_.find(id) != display_info_.end());
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  gfx::Rect bounds_in_pixel(display_info.size_in_pixel());

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the secondary display's bounds will be updated
  // by |DisplayController::UpdateDisplayBoundsForLayout|.
  new_display.SetScaleAndBounds(
      display_info.device_scale_factor(), gfx::Rect(bounds_in_pixel.size()));
  new_display.set_rotation(display_info.rotation());
  return new_display;
}

}  // namespace internal
}  // namespace ash
