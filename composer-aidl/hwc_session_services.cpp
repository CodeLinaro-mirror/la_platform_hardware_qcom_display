/*
* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <core/buffer_allocator.h>
#include <utils/debug.h>
#include <sync/sync.h>
#include <vector>
#include <string>
#include <errno.h>
#include <math.h>

#include "hwc_buffer_sync_handler.h"
#include "hwc_session.h"
#include "hwc_debugger.h"

#define __CLASS__ "HWCSession"

namespace sdm {

int MapDisplayType(DispType dpy) {
  switch (dpy) {
    case DispType::kPrimary:
      return qdutils::DISPLAY_PRIMARY;

    case DispType::kExternal:
      return qdutils::DISPLAY_EXTERNAL;

    case DispType::kVirtual:
      return qdutils::DISPLAY_VIRTUAL;

    default:
      break;
  }

  return -EINVAL;
}

HWCDisplay::DisplayStatus MapExternalStatus(DisplayConfig::ExternalStatus status) {
  switch (status) {
    case DisplayConfig::ExternalStatus::kOffline:
      return HWCDisplay::kDisplayStatusOffline;

    case DisplayConfig::ExternalStatus::kOnline:
      return HWCDisplay::kDisplayStatusOnline;

    case DisplayConfig::ExternalStatus::kPause:
      return HWCDisplay::kDisplayStatusPause;

    case DisplayConfig::ExternalStatus::kResume:
      return HWCDisplay::kDisplayStatusResume;

    default:
      break;
  }

  return HWCDisplay::kDisplayStatusInvalid;
}

int HWCSession::SetDisplayStatus(int disp_id, HWCDisplay::DisplayStatus status) {
  int disp_idx = GetDisplayIndex(disp_id);
  int err = -EINVAL;
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  if (disp_idx == qdutils::DISPLAY_PRIMARY) {
    DLOGE("Not supported for this display");
    return err;
  }

  {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
    if (!hwc_display_[disp_idx]) {
      DLOGW("Display is not connected");
      return err;
    }
    DLOGI("Display = %d, Status = %d", disp_idx, status);
    err = hwc_display_[disp_idx]->SetDisplayStatus(status);
    if (err != 0) {
      return err;
    }
  }

  if (status == HWCDisplay::kDisplayStatusResume || status == HWCDisplay::kDisplayStatusPause) {
    Display active_builtin_disp_id = GetActiveBuiltinDisplay();
    if (active_builtin_disp_id < HWCCallbacks::kNumDisplays) {
      {
        SEQUENCE_WAIT_SCOPE_LOCK(locker_[active_builtin_disp_id]);
        hwc_display_[active_builtin_disp_id]->ResetValidation();
      }
      callbacks_.Refresh(active_builtin_disp_id);
    }
  }

  return err;
}

int HWCSession::GetConfigCount(int disp_id, uint32_t *count) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);

  if (hwc_display_[disp_idx]) {
    return hwc_display_[disp_idx]->GetDisplayConfigCount(count);
  }

  return -EINVAL;
}

int HWCSession::GetActiveConfigIndex(int disp_id, uint32_t *config) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);

  if (hwc_display_[disp_idx]) {
    return hwc_display_[disp_idx]->GetActiveDisplayConfig(config);
  }

  return -EINVAL;
}

int HWCSession::SetActiveConfigIndex(int disp_id, uint32_t config) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  int error = -EINVAL;
  if (hwc_display_[disp_idx]) {
    error = hwc_display_[disp_idx]->SetActiveDisplayConfig(config);
    if (!error) {
      Refresh(0);
    }
  }

  return error;
}

int HWCSession::MinHdcpEncryptionLevelChanged(int disp_id, uint32_t min_enc_level) {
  DLOGI("Display %d", disp_id);

  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  if (disp_idx != HWC_DISPLAY_EXTERNAL) {
    DLOGE("Not supported for display");
  } else if (!hwc_display_[disp_idx]) {
    DLOGW("Display is not connected");
  } else {
    return hwc_display_[disp_idx]->OnMinHdcpEncryptionLevelChange(min_enc_level);
  }

  return -EINVAL;
}

int HWCSession::ControlPartialUpdate(int disp_id, bool enable) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  if (disp_idx != HWC_DISPLAY_PRIMARY) {
    DLOGW("CONTROL_PARTIAL_UPDATE is not applicable for display = %d", disp_idx);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  HWCDisplay *hwc_display = hwc_display_[HWC_DISPLAY_PRIMARY];
  if (!hwc_display) {
    DLOGE("primary display object is not instantiated");
    return -EINVAL;
  }

  uint32_t pending = 0;
  DisplayError hwc_error = hwc_display->ControlPartialUpdate(enable, &pending);

  if (hwc_error == kErrorNone) {
    if (!pending) {
      return 0;
    }
  } else if (hwc_error == kErrorNotSupported) {
    return 0;
  } else {
    return -EINVAL;
  }

  // Todo(user): Unlock it before sending events to client. It may cause deadlocks in future.
  Refresh(HWC_DISPLAY_PRIMARY);

  // Wait until partial update control is complete
  int error = locker_[disp_idx].WaitFinite(kCommitDoneTimeoutMs);

  return error;
}

int HWCSession::ToggleScreenUpdate(bool on) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  int32_t error = -EINVAL;
  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    error = hwc_display_[HWC_DISPLAY_PRIMARY]->ToggleScreenUpdates(on);
    if (error) {
      DLOGE("Failed to toggle screen updates = %d. Error = %d", on, error);
    }
  }

  return error;
}

int HWCSession::SetIdleTimeout(uint32_t value) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    hwc_display_[HWC_DISPLAY_PRIMARY]->SetIdleTimeoutMs(value);
    return 0;
  }

  DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
  return -ENODEV;
}

int HWCSession::SetCameraLaunchStatus(uint32_t on) {
  Display active_builtin_disp_id = GetActiveBuiltinDisplay();
  if (active_builtin_disp_id >= HWCCallbacks::kNumDisplays) {
    DLOGE("No active displays");
    return -EINVAL;
  }
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[active_builtin_disp_id]);

  if (null_display_mode_) {
    return 0;
  }

  if (!core_intf_) {
    DLOGW("core_intf_ not initialized.");
    return -ENOENT;
  }

  if (!hwc_display_[active_builtin_disp_id]) {
    DLOGW("Display = %d is not connected.", active_builtin_disp_id);
    return -ENODEV;
  }

  HWBwModes mode = on > 0 ? kBwCamera : kBwDefault;

  // trigger invalidate to apply new bw caps.
  Refresh(active_builtin_disp_id);

  if (core_intf_->SetMaxBandwidthMode(mode) != kErrorNone) {
    return -EINVAL;
  }

  new_bw_mode_ = true;
  need_invalidate_ = true;
  hwc_display_[active_builtin_disp_id]->ResetValidation();

  return 0;
}

int HWCSession::DisplayBWTransactionPending(bool *status) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    if (sync_wait(bw_mode_release_fd_, 0) < 0) {
      DLOGI("bw_transaction_release_fd is not yet signaled: err= %s", strerror(errno));
      *status = false;
    }

    return 0;
  }

  DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
  return -ENODEV;
}

int HWCSession::ControlIdlePowerCollapse(bool enable, bool synchronous) {
  Display active_builtin_disp_id = GetActiveBuiltinDisplay();
  if (active_builtin_disp_id >= HWCCallbacks::kNumDisplays) {
    DLOGE("No active displays");
    return -EINVAL;
  }
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[active_builtin_disp_id]);

  if (hwc_display_[active_builtin_disp_id]) {
    if (!enable) {
      if (!idle_pc_ref_cnt_) {
        auto err = hwc_display_[active_builtin_disp_id]->ControlIdlePowerCollapse(enable,
                                                                                  synchronous);
        if (err != kErrorNone) {
          return (err == kErrorNotSupported) ? 0 : -EINVAL;
        }
        Refresh(active_builtin_disp_id);
        int error = locker_[active_builtin_disp_id].WaitFinite(kCommitDoneTimeoutMs);
        if (error == ETIMEDOUT) {
          DLOGE("Timed out!! Next frame commit done event not received!!");
          return error;
        }
        DLOGI("Idle PC disabled!!");
      }
      idle_pc_ref_cnt_++;
    } else if (idle_pc_ref_cnt_ > 0) {
      if (!(idle_pc_ref_cnt_ - 1)) {
        auto err = hwc_display_[active_builtin_disp_id]->ControlIdlePowerCollapse(enable,
                                                                                  synchronous);
        if (err != kErrorNone) {
          return (err == kErrorNotSupported) ? 0 : -EINVAL;
        }
        DLOGI("Idle PC enabled!!");
      }
      idle_pc_ref_cnt_--;
    }
    return 0;
  }

  DLOGW("Display = %d is not connected.", active_builtin_disp_id);
  return -ENODEV;
}

int HWCSession::IsWbUbwcSupported(bool *value) {
  HWDisplaysInfo hw_displays_info = {};
  DisplayError error = core_intf_->GetDisplaysStatus(&hw_displays_info);
  if (error != kErrorNone) {
    return -EINVAL;
  }

  for (auto &iter : hw_displays_info) {
    auto &info = iter.second;
    if (info.display_type == kVirtual && info.is_wb_ubwc_supported) {
      *value = 1;
    }
  }

  return error;
}

int HWCSession::SetDisplayDppsAdROI(uint32_t display_id, uint32_t h_start,
                                    uint32_t h_end, uint32_t v_start, uint32_t v_end,
                                    uint32_t factor_in, uint32_t factor_out) {
  return static_cast<int> (CallDisplayFunction(display_id,
                             &HWCDisplay::SetDisplayDppsAdROI, h_start, h_end, v_start, v_end,
                             factor_in, factor_out));
}

bool HWCSession::isSmartPanelConfig(uint32_t disp_id, uint32_t config_id) {
  if (disp_id != qdutils::DISPLAY_PRIMARY) {
    return false;
  }

  SCOPE_LOCK(locker_[disp_id]);
  if (!hwc_display_[disp_id]) {
    DLOGE("Display %d is not created yet.", disp_id);
    return false;
  }

  return hwc_display_[disp_id]->IsSmartPanelConfig(config_id);
}

int32_t HWCSession::getDisplayBrightness(uint32_t display, float *brightness) {
  if (!brightness) {
    return static_cast<int32_t> (HWC3::Error::BadParameter);
  }

  if (display >= HWCCallbacks::kNumDisplays) {
    return static_cast<int32_t> (HWC3::Error::BadDisplay);
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[display]);
  int32_t error = -EINVAL;
  *brightness = -1.0f;

  HWCDisplay *hwc_display = hwc_display_[display];
  if (hwc_display && hwc_display_[display]->GetDisplayClass() == DISPLAY_CLASS_BUILTIN) {
    error = INT32(hwc_display_[display]->GetPanelBrightness(brightness));
    if (error) {
      DLOGE("Failed to get the panel brightness. Error = %d", error);
    }
  }

  return error;
}

int32_t HWCSession::setDisplayBrightness(uint32_t display, float brightness) {
  return static_cast<int32_t> (SetDisplayBrightness(static_cast<Display>(display),
                               brightness));
}

}  // namespace sdm
