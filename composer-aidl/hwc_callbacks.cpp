/*
 * Copyright (c) 2016-2017, 2019-2020 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <utils/constants.h>
#include <utils/debug.h>
#include "hwc_callbacks.h"

#define __CLASS__ "HWCCallbacks"

namespace sdm {

HWC3::Error HWCCallbacks::Hotplug(Display display, bool state) {
  std::lock_guard<std::mutex> hotplug_lock(hotplug_mutex_);
  if (!hotplug_) {
    return HWC3::Error::NoResources;
  }
  (*hotplug_)(hotplug_data_, display, INT32(state));
    return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Refresh(Display display) {
  std::lock_guard<std::mutex> refresh_lock(refresh_mutex_);
  if (!refresh_) {
    return HWC3::Error::NoResources;
  }
  (*refresh_)(refresh_data_, display);
  pending_refresh_.set(UINT32(display));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Vsync(Display display, int64_t timestamp, uint32_t period) {
  std::lock_guard<std::mutex> vsync_lock(vsync_mutex_);
  if (!vsync_) {
    return HWC3::Error::NoResources;
  }
  DTRACE_SCOPED();
  (*vsync_)(vsync_data_, static_cast<long>(display), timestamp, static_cast<int>(period));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::VsyncPeriodTimingChanged(
    Display display, VsyncPeriodChangeTimeline *updated_timeline) {
  std::lock_guard<std::mutex>
    vsyncPeriodTimingChanged_lock(vsync_period_timing_changed_mutex_);
  DTRACE_SCOPED();
  if (!vsync_period_timing_changed_) {
    return HWC3::Error::NoResources;
  }

  (*vsync_period_timing_changed_)(vsync_period_timing_changed_data_, static_cast<long>(display), *updated_timeline);
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::SeamlessPossible(Display display) {
  std::lock_guard<std::mutex> seamlessPossible_lock(seamless_possible_mutex_);
  DTRACE_SCOPED();
  if (!seamless_possible_) {
    return HWC3::Error::NoResources;
  }

  (*seamless_possible_)(seamless_possible_data_, static_cast<long>(display));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Register(CallbackCommand descriptor, void *callback_data,
                                   void *pointer) {
  switch (descriptor) {
    case CALLBACK_HOTPLUG:
      {
        std::lock_guard<std::mutex> hotplug_lock(hotplug_mutex_);
        hotplug_data_ = callback_data;
        hotplug_ = static_cast<onHotplug_func_t *>(pointer);
      }
      break;
    case CALLBACK_REFRESH:
      {
        std::lock_guard<std::mutex> refresh_lock(refresh_mutex_);
        refresh_data_ = callback_data;
        refresh_ = static_cast<onRefresh_func_t *>(pointer);
      }
      break;
    case CALLBACK_VSYNC:
      {
        std::lock_guard<std::mutex> vsync_lock(vsync_mutex_);
        vsync_data_ = callback_data;
        vsync_ = static_cast<onVsync_func_t *>(pointer);
      }
      break;
    case CALLBACK_VSYNC_PERIOD_TIMING_CHANGED:
      {
        std::lock_guard<std::mutex>
          vsyncPeriodTimingChanged_lock(vsync_period_timing_changed_mutex_);
        vsync_period_timing_changed_data_ = callback_data;
        vsync_period_timing_changed_ =
                static_cast<onVsyncPeriodTimingChanged_func_t *>(pointer);
      }
      break;
    case CALLBACK_SEAMLESS_POSSIBLE:
      {
        std::lock_guard<std::mutex> seamlessPossible_lock(seamless_possible_mutex_);
        seamless_possible_data_ = callback_data;
        seamless_possible_ = static_cast<onSeamlessPossible_func_t *>(pointer);
      }
      break;
    default:
      return HWC3::Error::BadParameter;
  }
  return HWC3::Error::None;
}

}  // namespace sdm
