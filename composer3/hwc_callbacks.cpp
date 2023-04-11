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

#include <thread>

#include <utils/constants.h>
#include <utils/debug.h>
#include <utils/locker.h>
#include "hwc_callbacks.h"

#define __CLASS__ "HWCCallbacks"

namespace sdm {

HWC3::Error HWCCallbacks::Hotplug(Display display, bool state) {
  SCOPE_LOCK(hotplug_lock_);
  DTRACE_SCOPED();

  // If client has not registered hotplug, wait for it finitely:
  //   1. spurious wake up (!hotplug_), wait again
  //   2. error = ETIMEDOUT, return with warning 1
  //   3. error != ETIMEDOUT, return with warning 2
  //   4. error == NONE and no spurious wake up, then !hotplug_ is false, exit loop
  while (!hotplug_) {
    DLOGW("Attempting to send client a hotplug too early Display = %" PRIu64 " state = %d", display,
          state);
    int ret = hotplug_lock_.WaitFinite(5000);
    if (ret == ETIMEDOUT) {
      DLOGW("Client didn't connect on time, dropping hotplug!");
      return HWC3::Error::None;
    } else if (ret != 0) {
      DLOGW("Failed client connection wait. Error %s, dropping hotplug!", strerror(ret));
      return HWC3::Error::None;
    }
  }
  // External display hotplug events are handled asynchronously
  if (display == HWC_DISPLAY_EXTERNAL || display == HWC_DISPLAY_EXTERNAL_2) {
    std::thread(*hotplug_, callback_data_, static_cast<long>(display), INT32(state)).detach();
  } else {
    (*hotplug_)(callback_data_, display, INT32(state));
  }
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Refresh(Display display) {
  SCOPE_LOCK(refresh_lock_);
  // Do not lock, will cause hotplug deadlock
  DTRACE_SCOPED();
  // If client has not registered refresh, drop it
  if (!refresh_) {
    return HWC3::Error::NoResources;
  }
  std::thread (*refresh_, callback_data_, static_cast<long>(display)).detach();
  pending_refresh_.set(UINT32(display));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Vsync(Display display, int64_t timestamp, uint32_t period) {
  SCOPE_LOCK(vsync_lock_);
  // Do not lock, may cause hotplug deadlock
  DTRACE_SCOPED();
  // If client has not registered vsync, drop it
  if (!vsync_) {
    return HWC3::Error::NoResources;
  }
  (*vsync_)(callback_data_, static_cast<long>(display), timestamp, static_cast<int>(period));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::VsyncIdle(Display display) {
  SCOPE_LOCK(vsync_idle_lock_);
  // Do not lock, may cause hotplug deadlock
  DTRACE_SCOPED();
  // If client has not registered vsync, drop it
  if (!vsync_idle_) {
    return HWC3::Error::NoResources;
  }
  (*vsync_idle_)(callback_data_, static_cast<long>(display));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::VsyncPeriodTimingChanged(Display display,
                                                   VsyncPeriodChangeTimeline *updated_timeline) {
  SCOPE_LOCK(vsync_changed_lock_);
  DTRACE_SCOPED();
  if (!vsync_changed_) {
    return HWC3::Error::NoResources;
  }

  (*vsync_changed_)(callback_data_, static_cast<long>(display), *updated_timeline);
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::SeamlessPossible(Display display) {
  SCOPE_LOCK(seamless_possible_lock_);
  DTRACE_SCOPED();
  if (!seamless_possible_) {
    return HWC3::Error::NoResources;
  }

  (*seamless_possible_)(callback_data_, static_cast<long>(display));
  return HWC3::Error::None;
}

HWC3::Error HWCCallbacks::Register(CallbackCommand descriptor, void *callback_data, void *pointer) {
  switch (descriptor) {
    case CALLBACK_HOTPLUG: {
      SCOPE_LOCK(hotplug_lock_);
      hotplug_ = static_cast<onHotplug_func_t *>(pointer);
      client_connected_ = true;
      hotplug_lock_.Broadcast();
    } break;
    case CALLBACK_REFRESH: {
      SCOPE_LOCK(refresh_lock_);
      refresh_ = static_cast<onRefresh_func_t *>(pointer);
    } break;
    case CALLBACK_VSYNC: {
      SCOPE_LOCK(vsync_lock_);
      vsync_ = static_cast<onVsync_func_t *>(pointer);
    } break;
    case CALLBACK_VSYNC_IDLE: {
      SCOPE_LOCK(vsync_idle_lock_);
      vsync_idle_ = static_cast<onVsyncIdle_func_t *>(pointer);
    } break;
    case CALLBACK_VSYNC_PERIOD_TIMING_CHANGED: {
      SCOPE_LOCK(vsync_changed_lock_);
      vsync_changed_ = static_cast<onVsyncPeriodTimingChanged_func_t *>(pointer);
    } break;
    case CALLBACK_SEAMLESS_POSSIBLE: {
      SCOPE_LOCK(seamless_possible_lock_);
      seamless_possible_ = static_cast<onSeamlessPossible_func_t *>(pointer);
    } break;
    default:
      return HWC3::Error::BadParameter;
  }
  callback_data_ = callback_data;

  return HWC3::Error::None;
}

}  // namespace sdm
