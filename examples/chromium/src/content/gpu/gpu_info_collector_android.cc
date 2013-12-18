// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "cc/base/switches.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/android/device_display_info.h"

namespace {

std::string GetDriverVersionFromString(const std::string& version_string) {
  // Extract driver version from the second number in a string like:
  // "OpenGL ES 2.0 V@6.0 AU@ (CL@2946718)"

  // Exclude first "2.0".
  size_t begin = version_string.find_first_of("0123456789");
  if (begin == std::string::npos)
    return "0";
  size_t end = version_string.find_first_not_of("01234567890.", begin);

  // Extract number of the form "%d.%d"
  begin = version_string.find_first_of("0123456789", end);
  if (begin == std::string::npos)
    return "0";
  end = version_string.find_first_not_of("01234567890.", begin);
  std::string sub_string;
  if (end != std::string::npos)
    sub_string = version_string.substr(begin, end - begin);
  else
    sub_string = version_string.substr(begin);
  std::vector<std::string> pieces;
  base::SplitString(sub_string, '.', &pieces);
  if (pieces.size() < 2)
    return "0";
  return pieces[0] + "." + pieces[1];
}

}

namespace gpu_info_collector {

bool CollectContextGraphicsInfo(content::GPUInfo* gpu_info) {
  // can_lose_context must be false to enable accelerated Canvas2D
  gpu_info->can_lose_context = false;
  gpu_info->finalized = true;
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectGpuID(uint32* vendor_id, uint32* device_id) {
  DCHECK(vendor_id && device_id);
  *vendor_id = 0;
  *device_id = 0;
  return false;
}

bool CollectBasicGraphicsInfo(content::GPUInfo* gpu_info) {
  gpu_info->can_lose_context = false;
  // Create a short-lived context on the UI thread to collect the GL strings.
  if (!CollectGraphicsInfoGL(gpu_info))
    return false;

  std::string vendor(StringToLowerASCII(gpu_info->gl_vendor));
  std::string renderer(StringToLowerASCII(gpu_info->gl_renderer));
  bool is_img = vendor.find("imagination") != std::string::npos;
  bool is_arm = vendor.find("arm") != std::string::npos;
  bool is_qualcomm = vendor.find("qualcomm") != std::string::npos;
  bool is_mali_t604 = is_arm && renderer.find("mali-t604") != std::string::npos;
  bool is_hisilicon = vendor.find("hisilicon") != std::string::npos;

  base::android::BuildInfo* build_info =
      base::android::BuildInfo::GetInstance();
  std::string model = build_info->model();
  model = StringToLowerASCII(model);
  bool is_nexus7 = model.find("nexus 7") != std::string::npos;
  bool is_nexus10 = model.find("nexus 10") != std::string::npos;

  // Virtual contexts crash when switching surfaces, but only on SDK 16,
  // so we us full context switching for now. If virtual contexts can
  // avoid some of it's extra surface switches, or Qualcomm gives us
  // a more directed work-around, we can remove this.
  int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();

  // IMG: avoid context switching perf problems, crashes with share groups
  // Mali-T604: http://crbug.com/154715
  // QualComm, NVIDIA: Crashes with share groups
  if (is_hisilicon || is_img || is_mali_t604 || is_nexus7
      || (is_qualcomm && sdk_int != 16)) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableVirtualGLContexts);
  }

  gfx::DeviceDisplayInfo info;
  int default_tile_size = 256;

  // For very high resolution displays (eg. Nexus 10), set the default
  // tile size to be 512. This should be removed in favour of a generic
  // hueristic that works across all platforms and devices, once that
  // exists: http://crbug.com/159524. This switches to 512 for screens
  // containing 40 or more 256x256 tiles, such that 1080p devices do
  // not use 512x512 tiles (eg. 1920x1280 requires 37.5 tiles)
  int numTiles = (info.GetDisplayWidth() *
                  info.GetDisplayHeight()) / (256 * 256);
  if (numTiles >= 40)
    default_tile_size = 512;

  // IMG: Fast async texture uploads only work with non-power-of-two,
  // but still multiple-of-eight sizes.
  // http://crbug.com/168099
  if (is_img)
    default_tile_size -= 8;

  // Set the command line if it isn't already set and we changed
  // the default tile size.
  if (default_tile_size != 256 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDefaultTileWidth) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDefaultTileHeight)) {
    std::stringstream size;
    size << default_tile_size;
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDefaultTileWidth, size.str());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDefaultTileHeight, size.str());
  }

  // Increase the resolution of low resolution tiles for Nexus tablets.
  if ((is_nexus7 || is_nexus10) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kLowResolutionContentsScaleFactor)) {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        cc::switches::kLowResolutionContentsScaleFactor, "0.25");
  }

  return true;
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  gpu_info->driver_version = GetDriverVersionFromString(
      gpu_info->gl_version_string);
  return true;
}

void MergeGPUInfo(content::GPUInfo* basic_gpu_info,
                  const content::GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

}  // namespace gpu_info_collector
