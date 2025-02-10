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
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __HWC_CALLBACKS_H__
#define __HWC_CALLBACKS_H__

#include <thread>
#include <mutex>
#include <queue>
#include <utils/locker.h>
#include "hwc_common.h"

namespace sdm {

template <typename T>
class CallbackQueue {
 public:
  void push(const T &inbuf) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      queue_.push(inbuf);
    }
    cond_.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      cond_.wait(lock);
    }
    T head = queue_.front();
    queue_.pop();
    return head;
  }

  bool empty() {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  void wait(bool *cond) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [&] { return !queue_.empty() || !*cond; });
  }

  void notify() { cond_.notify_one(); }

 private:
  std::mutex mutex_;
  std::condition_variable cond_;
  std::queue<T> queue_;
};

class HWCCallbacks {
 public:
  static const int kNumBuiltIn = 4;
  static const int kNumPluggable = 4;
  static const int kNumVirtual = 4;
  // Add 1 primary display which can be either a builtin or pluggable.
  // Async powermode update requires dummy hwc displays.
  // Limit dummy displays to builtin/pluggable type for now.
  static const int kNumRealDisplays = 1 + kNumBuiltIn + kNumPluggable + kNumVirtual;
  static const int kNumDisplays =
      1 + kNumBuiltIn + kNumPluggable + kNumVirtual + 1 + kNumBuiltIn + kNumPluggable;

  HWC3::Error Hotplug(Display display, bool state);
  HWC3::Error Refresh(Display display);
  HWC3::Error Vsync(Display display, int64_t timestamp, uint32_t period);
  HWC3::Error VsyncIdle(Display display);
  HWC3::Error VsyncPeriodTimingChanged(Display display,
                                       VsyncPeriodChangeTimeline *updated_timeline);
  HWC3::Error SeamlessPossible(Display display);
  HWC3::Error Register(CallbackCommand descriptor, void *callback_data, void *pointer);
  void UpdateVsyncSource(Display from) { vsync_source_ = from; }
  Display GetVsyncSource() { return vsync_source_; }

  bool VsyncCallbackRegistered() { return (vsync_ != nullptr && callback_data_ != nullptr); }
  bool NeedsRefresh(Display display) { return pending_refresh_.test(UINT32(display)); }
  void ResetRefresh(Display display) { pending_refresh_.reset(UINT32(display)); }
  bool IsClientConnected() {
    SCOPE_LOCK(hotplug_lock_);
    return client_connected_;
  }
  HWC3::Error PerformHWCCallback();

 private:
  struct HWCCallbackParams {
    CallbackCommand cmd;
    long display;
    int32_t state;
  };

  void *callback_data_ = nullptr;

  onHotplug_func_t *hotplug_ = nullptr;
  onRefresh_func_t *refresh_ = nullptr;
  onVsync_func_t *vsync_ = nullptr;
  onSeamlessPossible_func_t *seamless_possible_ = nullptr;
  onVsyncPeriodTimingChanged_func_t *vsync_changed_ = nullptr;
  onVsyncIdle_func_t *vsync_idle_ = nullptr;

  Display vsync_source_ = HWC_DISPLAY_PRIMARY;  // hw vsync is active on this display
  std::bitset<kNumDisplays> pending_refresh_;   // Displays waiting to get refreshed

  Locker hotplug_lock_;
  Locker refresh_lock_;
  Locker vsync_lock_;
  Locker vsync_idle_lock_;
  Locker vsync_changed_lock_;
  Locker seamless_possible_lock_;
  bool client_connected_ = false;
  std::thread callback_thread_;
  CallbackQueue<HWCCallbackParams> callback_queue_;
  bool callback_running_ = false;
};

}  // namespace sdm

#endif  // __HWC_CALLBACKS_H__
