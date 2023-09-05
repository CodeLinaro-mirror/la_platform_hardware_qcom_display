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

#ifndef __HWC_CALLBACKS_H__
#define __HWC_CALLBACKS_H__

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <mutex>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11
#include "hwc_common.h"

namespace sdm {

class HWCCallbacks {
 public:
  static const int kNumBuiltIn = 4;
  static const int kNumPluggable = 4;
  static const int kNumVirtual = 4;
  // Add 1 primary display which can be either a builtin or pluggable.
  // Async powermode update requires dummy hwc displays.
  // Limit dummy displays to builtin/pluggable type for now.
  static const int kNumRealDisplays = 1 + kNumBuiltIn + kNumPluggable + kNumVirtual;
  static const int kNumDisplays = 1 + kNumBuiltIn + kNumPluggable + kNumVirtual +
                                    1 + kNumBuiltIn + kNumPluggable;

  HWC3::Error Hotplug(Display display, bool state);
  HWC3::Error Refresh(Display display);
  HWC3::Error Vsync(Display display, int64_t timestamp, uint32_t period);
  HWC3::Error VsyncPeriodTimingChanged(Display display,
                                       VsyncPeriodChangeTimeline *updated_timeline);
  HWC3::Error SeamlessPossible(Display display);
  HWC3::Error Register(CallbackCommand descriptor, void *callback_data, void *pointer);
  void UpdateVsyncSource(Display from) {
    vsync_source_ = from;
  }
  Display GetVsyncSource() { return vsync_source_; }

  bool VsyncCallbackRegistered() { return (vsync_ != nullptr && vsync_data_ != nullptr); }
  bool NeedsRefresh(Display display) { return pending_refresh_.test(UINT32(display)); }
  void ResetRefresh(Display display) { pending_refresh_.reset(UINT32(display)); }

 private:
  void *callback_data_ = nullptr;
  void *hotplug_data_ = nullptr;
  void * refresh_data_ = nullptr;
  void * vsync_data_ = nullptr;
  void * vsync_period_timing_changed_data_ = nullptr;
  void * seamless_possible_data_ = nullptr;

  onHotplug_func_t *hotplug_ = nullptr;
  onRefresh_func_t *refresh_ = nullptr;
  onVsync_func_t *vsync_ = nullptr;
  onVsyncPeriodTimingChanged_func_t *vsync_period_timing_changed_ = nullptr;
  onSeamlessPossible_func_t *seamless_possible_ = nullptr;

  std::mutex hotplug_mutex_;
  std::mutex refresh_mutex_;
  std::mutex vsync_mutex_;
  std::mutex vsync_2_4_mutex_;
  std::mutex vsync_period_timing_changed_mutex_;
  std::mutex seamless_possible_mutex_;

  Display vsync_source_ = HWC_DISPLAY_PRIMARY;   // hw vsync is active on this display
  std::bitset<kNumDisplays> pending_refresh_;         // Displays waiting to get refreshed
};

}  // namespace sdm

#endif  // __HWC_CALLBACKS_H__
