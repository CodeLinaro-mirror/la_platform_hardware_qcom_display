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
*
* Changes from Qualcomm Innovation Center are provided under the following license:
*
* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*
*    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <core/buffer_allocator.h>
#include <utils/debug.h>
#include <sync/sync.h>
#include <vector>
#include <string>

#include "hwc_buffer_sync_handler.h"
#include "hwc_session.h"
#include "hwc_debugger.h"

#define __CLASS__ "HWCSession"

namespace sdm {

using ::android::hardware::Void;

void HWCSession::StartServices() {
  android::status_t status = IDisplayConfig::registerAsService();
  if (status != android::OK) {
    DLOGW("Could not register IDisplayConfig as service (%d).", status);
  } else {
    DLOGI("IDisplayConfig service registration completed.");
  }
}

int MapDisplayType(IDisplayConfig::DisplayType dpy) {
  switch (dpy) {
    case IDisplayConfig::DisplayType::DISPLAY_PRIMARY:
      return qdutils::DISPLAY_PRIMARY;

    case IDisplayConfig::DisplayType::DISPLAY_EXTERNAL:
      return qdutils::DISPLAY_EXTERNAL;

    case IDisplayConfig::DisplayType::DISPLAY_VIRTUAL:
      return qdutils::DISPLAY_VIRTUAL;

    default:
      break;
  }

  return -EINVAL;
}

HWCDisplay::DisplayStatus MapExternalStatus(IDisplayConfig::DisplayExternalStatus status) {
  switch (status) {
    case IDisplayConfig::DisplayExternalStatus::EXTERNAL_OFFLINE:
      return HWCDisplay::kDisplayStatusOffline;

    case IDisplayConfig::DisplayExternalStatus::EXTERNAL_ONLINE:
      return HWCDisplay::kDisplayStatusOnline;

    case IDisplayConfig::DisplayExternalStatus::EXTERNAL_PAUSE:
      return HWCDisplay::kDisplayStatusPause;

    case IDisplayConfig::DisplayExternalStatus::EXTERNAL_RESUME:
      return HWCDisplay::kDisplayStatusResume;

    default:
      break;
  }

  return HWCDisplay::kDisplayStatusInvalid;
}

// Methods from ::vendor::hardware::display::config::V1_0::IDisplayConfig follow.
Return<void> HWCSession::isDisplayConnected(IDisplayConfig::DisplayType dpy,
                                            isDisplayConnected_cb _hidl_cb) {
  int32_t error = -EINVAL;
  bool connected = false;
  int disp_id = MapDisplayType(dpy);
  int disp_idx = GetDisplayIndex(disp_id);

  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
  } else {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
    connected = hwc_display_[disp_idx];
    error = 0;
  }
  _hidl_cb(error, connected);

  return Void();
}

int32_t HWCSession::SetSecondaryDisplayStatus(int disp_id, HWCDisplay::DisplayStatus status) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  DLOGI("Display = %d, Status = %d", disp_idx, status);

  if (disp_idx == qdutils::DISPLAY_PRIMARY) {
    DLOGE("Not supported for this display");
  } else if (!hwc_display_[disp_idx]) {
    DLOGW("Display is not connected");
  } else {
    return hwc_display_[disp_idx]->SetDisplayStatus(status);
  }

  return -EINVAL;
}

Return<int32_t> HWCSession::setSecondayDisplayStatus(IDisplayConfig::DisplayType dpy,
                                                  IDisplayConfig::DisplayExternalStatus status) {
  return SetSecondaryDisplayStatus(MapDisplayType(dpy), MapExternalStatus(status));
}

Return<int32_t> HWCSession::configureDynRefeshRate(IDisplayConfig::DisplayDynRefreshRateOp op,
                                                   uint32_t refreshRate) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
  HWCDisplay *hwc_display = hwc_display_[HWC_DISPLAY_PRIMARY];

  switch (op) {
    case IDisplayConfig::DisplayDynRefreshRateOp::DISABLE_METADATA_DYN_REFRESH_RATE:
      return hwc_display->Perform(HWCDisplayBuiltIn::SET_METADATA_DYN_REFRESH_RATE, false);

    case IDisplayConfig::DisplayDynRefreshRateOp::ENABLE_METADATA_DYN_REFRESH_RATE:
      return hwc_display->Perform(HWCDisplayBuiltIn::SET_METADATA_DYN_REFRESH_RATE, true);

    case IDisplayConfig::DisplayDynRefreshRateOp::SET_BINDER_DYN_REFRESH_RATE:
      return hwc_display->Perform(HWCDisplayBuiltIn::SET_BINDER_DYN_REFRESH_RATE, refreshRate);

    default:
      DLOGW("Invalid operation %d", op);
      return -EINVAL;
  }

  return 0;
}

int32_t HWCSession::GetConfigCount(int disp_id, uint32_t *count) {
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

Return<void> HWCSession::getConfigCount(IDisplayConfig::DisplayType dpy,
                                        getConfigCount_cb _hidl_cb) {
  uint32_t count = 0;
  int32_t error = GetConfigCount(MapDisplayType(dpy), &count);

  _hidl_cb(error, count);

  return Void();
}

int32_t HWCSession::GetActiveConfigIndex(int disp_id, uint32_t *config) {
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

Return<void> HWCSession::getActiveConfig(IDisplayConfig::DisplayType dpy,
                                         getActiveConfig_cb _hidl_cb) {
  uint32_t config = 0;
  int32_t error = GetActiveConfigIndex(MapDisplayType(dpy), &config);

  _hidl_cb(error, config);

  return Void();
}

int32_t HWCSession::SetActiveConfigIndex(int disp_id, uint32_t config) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  int32_t error = -EINVAL;
  if (hwc_display_[disp_idx]) {
    error = hwc_display_[disp_idx]->SetActiveDisplayConfig(config);
    if (!error) {
      Refresh(0);
    }
  }

  return error;
}

Return<int32_t> HWCSession::setActiveConfig(IDisplayConfig::DisplayType dpy, uint32_t config) {
  return SetActiveConfigIndex(MapDisplayType(dpy), config);
}

Return<void> HWCSession::getDisplayAttributes(uint32_t configIndex,
                                              IDisplayConfig::DisplayType dpy,
                                              getDisplayAttributes_cb _hidl_cb) {
  int32_t error = -EINVAL;
  IDisplayConfig::DisplayAttributes display_attributes = {};
  int disp_id = MapDisplayType(dpy);
  int disp_idx = GetDisplayIndex(disp_id);

  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
  } else {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
    if (hwc_display_[disp_idx]) {
      DisplayConfigVariableInfo var_info;
      error = hwc_display_[disp_idx]->GetDisplayAttributesForConfig(INT(configIndex), &var_info);
      if (!error) {
        display_attributes.vsyncPeriod = var_info.vsync_period_ns;
        display_attributes.xRes = var_info.x_pixels;
        display_attributes.yRes = var_info.y_pixels;
        display_attributes.xDpi = var_info.x_dpi;
        display_attributes.yDpi = var_info.y_dpi;
        display_attributes.panelType = IDisplayConfig::DisplayPortType::DISPLAY_PORT_DEFAULT;
        display_attributes.isYuv = var_info.is_yuv;
      }
    }
  }
  _hidl_cb(error, display_attributes);

  return Void();
}

Return<int32_t> HWCSession::setPanelBrightness(uint32_t level) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
  int32_t error = -EINVAL;

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    error = hwc_display_[HWC_DISPLAY_PRIMARY]->SetPanelBrightness(INT(level));
    if (error) {
      DLOGE("Failed to set the panel brightness = %d. Error = %d", level, error);
    }
  }

  return error;
}

int32_t HWCSession::GetPanelBrightness(int *level) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
  int32_t error = -EINVAL;

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    error = hwc_display_[HWC_DISPLAY_PRIMARY]->GetPanelBrightness(level);
    if (error) {
      DLOGE("Failed to get the panel brightness. Error = %d", error);
    }
  }

  return error;
}

Return<void> HWCSession::getPanelBrightness(getPanelBrightness_cb _hidl_cb) {
  int level = 0;
  int32_t error = GetPanelBrightness(&level);

  _hidl_cb(error, static_cast<uint32_t>(level));

  return Void();
}

int32_t HWCSession::MinHdcpEncryptionLevelChanged(int disp_id, uint32_t min_enc_level) {
  DLOGI("Display %d", disp_id);

  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  hwc2_display_t external_display_index =
    (hwc2_display_t)GetDisplayIndex(qdutils::DISPLAY_EXTERNAL);

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  if (disp_idx != external_display_index) {
    DLOGE("Not supported for display");
  } else if (!hwc_display_[disp_idx]) {
    DLOGW("Display is not connected");
  } else {
    return hwc_display_[disp_idx]->OnMinHdcpEncryptionLevelChange(min_enc_level);
  }

  return -EINVAL;
}

Return<int32_t> HWCSession::minHdcpEncryptionLevelChanged(IDisplayConfig::DisplayType dpy,
                                                          uint32_t min_enc_level) {
  return MinHdcpEncryptionLevelChanged(MapDisplayType(dpy), min_enc_level);
}

Return<int32_t> HWCSession::refreshScreen() {
  {
    SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
    if (!hwc_display_[HWC_DISPLAY_PRIMARY]) {
      DLOGE("primary display object is not instantiated");
      return -EINVAL;
    }
  }
  hwc_display_[HWC_DISPLAY_PRIMARY]->Refresh();
  return 0;
}

int32_t HWCSession::ControlPartialUpdate(int disp_id, bool enable) {
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
  int32_t error = locker_[disp_idx].WaitFinite(kPartialUpdateControlTimeoutMs);

  return error;
}

Return<int32_t> HWCSession::controlPartialUpdate(IDisplayConfig::DisplayType dpy, bool enable) {
  return ControlPartialUpdate(MapDisplayType(dpy), enable);
}

Return<int32_t> HWCSession::toggleScreenUpdate(bool on) {
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

Return<int32_t> HWCSession::setIdleTimeout(uint32_t value) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    hwc_display_[HWC_DISPLAY_PRIMARY]->SetIdleTimeoutMs(value);
    return 0;
  }

  DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
  return -ENODEV;
}

Return<void> HWCSession::getHDRCapabilities(IDisplayConfig::DisplayType dpy,
                                            getHDRCapabilities_cb _hidl_cb) {
  int32_t error = -EINVAL;
  IDisplayConfig::DisplayHDRCapabilities hdr_caps = {};

  do {
    if (!_hidl_cb) {
      DLOGE("_hidl_cb callback not provided.");
      break;
    }

    int disp_id = MapDisplayType(dpy);
    int disp_idx = GetDisplayIndex(disp_id);
    if (disp_idx == -1) {
      DLOGE("Invalid display = %d", disp_id);
      break;
    }

    SCOPE_LOCK(locker_[disp_id]);
    HWCDisplay *hwc_display = hwc_display_[disp_idx];
    if (!hwc_display) {
      DLOGW("Display = %d is not connected.", disp_idx);
      error = -ENODEV;
      break;
    }

    // query number of hdr types
    uint32_t out_num_types = 0;
    float out_max_luminance = 0.0f;
    float out_max_average_luminance = 0.0f;
    float out_min_luminance = 0.0f;
    if (hwc_display->GetHdrCapabilities(&out_num_types, nullptr, &out_max_luminance,
                                        &out_max_average_luminance, &out_min_luminance)
                                        != HWC2::Error::None) {
      break;
    }
    if (!out_num_types) {
      error = 0;
      break;
    }

    // query hdr caps
    hdr_caps.supportedHdrTypes.resize(out_num_types);

    if (hwc_display->GetHdrCapabilities(&out_num_types, hdr_caps.supportedHdrTypes.data(),
                                        &out_max_luminance, &out_max_average_luminance,
                                        &out_min_luminance) == HWC2::Error::None) {
      error = 0;
    }
  } while (false);

  _hidl_cb(error, hdr_caps);

  return Void();
}

Return<int32_t> HWCSession::setCameraLaunchStatus(uint32_t on) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  if (!core_intf_) {
    DLOGW("core_intf_ not initialized.");
    return -ENOENT;
  }

  if (!hwc_display_[HWC_DISPLAY_PRIMARY]) {
    DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
    return -ENODEV;
  }

  HWBwModes mode = on > 0 ? kBwCamera : kBwDefault;

  // trigger invalidate to apply new bw caps.
  Refresh(HWC_DISPLAY_PRIMARY);

  if (core_intf_->SetMaxBandwidthMode(mode) != kErrorNone) {
    return -EINVAL;
  }

  new_bw_mode_ = true;
  need_invalidate_ = true;
  hwc_display_[HWC_DISPLAY_PRIMARY]->ResetValidation();

  return 0;
}

int32_t HWCSession::DisplayBWTransactionPending(bool *status) {
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

Return<void> HWCSession::displayBWTransactionPending(displayBWTransactionPending_cb _hidl_cb) {
  bool status = true;

  if (!_hidl_cb) {
      DLOGE("_hidl_cb callback not provided.");
      return Void();
  }

  int32_t error = DisplayBWTransactionPending(&status);

  _hidl_cb(error, status);

  return Void();
}

#ifdef DISPLAY_CONFIG_1_1
Return<int32_t> HWCSession::setDisplayAnimating(uint64_t display_id, bool animating ) {
  return CallDisplayFunction(static_cast<hwc2_device_t *>(this), display_id,
                             &HWCDisplay::SetDisplayAnimating, animating);
}
#endif

#ifdef DISPLAY_CONFIG_1_2
Return<int32_t> HWCSession::setDisplayIndex(IDisplayConfig::DisplayTypeExt disp_type,
                                            uint32_t base, uint32_t count) {
  return -1;
}
#endif

#ifdef DISPLAY_CONFIG_1_3
Return<int32_t> HWCSession::controlIdlePowerCollapse(bool enable, bool synchronous) {
  DLOGW("Not implemented.");
  return 0;
}
#endif

#ifdef DISPLAY_CONFIG_1_5
Return<int32_t> HWCSession::SetDisplayDppsAdROI(uint32_t display_id, uint32_t h_start,
                                                uint32_t h_end, uint32_t v_start, uint32_t v_end,
                                                uint32_t factor_in, uint32_t factor_out) {
    DLOGW("Not implemented.");
    return 0;
}
#endif

#ifdef DISPLAY_CONFIG_1_6
Return<int32_t> HWCSession::updateVSyncSourceOnPowerModeOff() {
  return 0;
}

Return<int32_t> HWCSession::updateVSyncSourceOnPowerModeDoze() {
  return 0;
}
#endif

#ifdef DISPLAY_CONFIG_1_7
Return<int32_t> HWCSession::setPowerMode(uint32_t disp_id, PowerMode power_mode) {
  return 0;
}

Return<bool> HWCSession::isPowerModeOverrideSupported(uint32_t disp_id) {
  return false;
}

Return<bool> HWCSession::isHDRSupported(uint32_t disp_id) {
  if ((is_hdr_display_.size()==0) || (disp_id > (is_hdr_display_.size()-1))) {
    DLOGW("Not valid display. Id = %d",disp_id);
    return false;
  }

  return static_cast<bool>(is_hdr_display_[disp_id]);
}

Return<bool> HWCSession::isWCGSupported(uint32_t disp_id) {
  // todo(user): Query wcg from sdm. For now assume them same.
  return isHDRSupported(disp_id);
}

Return<int32_t> HWCSession::setLayerAsMask(uint32_t disp_id, uint64_t layer_id) {
  return 0;
}

Return<void> HWCSession::getDebugProperty(const hidl_string &prop_name,
                                          getDebugProperty_cb _hidl_cb) {
  std::string vendor_prop_name = DISP_PROP_PREFIX;
  char value[64] = {};
  hidl_string result = "";
  int32_t error = -EINVAL;

  vendor_prop_name += prop_name.c_str();
  if (HWCDebugHandler::Get()->GetProperty(vendor_prop_name.c_str(), value) != kErrorNone) {
    result = value;
    error = 0;
  }

  _hidl_cb(result, error);

  return Void();
}
#endif


int32_t HWCSession::IsWbUbwcSupported(int *value) {
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

#ifdef DISPLAY_CONFIG_1_4
Return<void> HWCSession::getWriteBackCapabilities(getWriteBackCapabilities_cb _hidl_cb) {
  int value = 0;
  IDisplayConfig::WriteBackCapabilities wb_caps = {};
  int32_t error = IsWbUbwcSupported(&value);
  wb_caps.isWbUbwcSupported = value;
  _hidl_cb(error, wb_caps);

  return Void();
}
#endif  // DISPLAY_CONFIG_1_4

#ifdef DISPLAY_CONFIG_1_8
Return<void> HWCSession::getActiveBuiltinDisplayAttributes(
                                          getDisplayAttributes_cb _hidl_cb) {
  int32_t error = -EINVAL;
  IDisplayConfig::DisplayAttributes display_attributes = {};
  hwc2_display_t disp_id = GetActiveBuiltinDisplay();

  if (disp_id >= HWCCallbacks::kNumDisplays) {
    DLOGE("Invalid display = %d", disp_id);
  } else {
    if (hwc_display_[disp_id]) {
      uint32_t config_index = 0;
      HWC2::Error ret = hwc_display_[disp_id]->GetActiveConfig(&config_index);
      if (ret != HWC2::Error::None) {
        goto err;
      }
      DisplayConfigVariableInfo var_info;
      error = hwc_display_[disp_id]->GetDisplayAttributesForConfig(INT(config_index), &var_info);
      if (!error) {
        display_attributes.vsyncPeriod = var_info.vsync_period_ns;
        display_attributes.xRes = var_info.x_pixels;
        display_attributes.yRes = var_info.y_pixels;
        display_attributes.xDpi = var_info.x_dpi;
        display_attributes.yDpi = var_info.y_dpi;
        display_attributes.panelType = IDisplayConfig::DisplayPortType::DISPLAY_PORT_DEFAULT;
        display_attributes.isYuv = var_info.is_yuv;
      }
    }
  }

err:
  _hidl_cb(error, display_attributes);

  return Void();
}
#endif  // DISPLAY_CONFIG_1_8

#ifdef DISPLAY_CONFIG_1_9
Return<int32_t> HWCSession::setPanelLuminanceAttributes(uint32_t disp_id, float pan_min_lum,
                                                        float pan_max_lum) {
  DLOGE("Not supported at present");
  return -1;
}

Return<bool> HWCSession::isBuiltInDisplay(uint32_t disp_id) {
  if ((map_info_primary_.client_id == disp_id) && (map_info_primary_.disp_type == kBuiltIn))
    return true;

  for (auto &info : map_info_builtin_) {
    if (disp_id == info.client_id) {
      return true;
    }
  }

  return false;
}
#endif  // DISPLAY_CONFIG_1_9

#ifdef DISPLAY_CONFIG_1_21
Return<int32_t> HWCSession::setCWBOutputBuffer(const sp<IDisplayCWBCallback>& callback,
                                    uint32_t disp_id, const IDisplayConfig::Rect& rect,
                                    bool post_processed, const hidl_handle& buffer) {
  return 0;
}

Return<void> HWCSession::getSupportedDSIBitClks(uint32_t disp_id,
                 IDisplayConfig::getSupportedDSIBitClks_cb _hidl_cb) {
  return Void();
}

Return<uint64_t> HWCSession::getDSIClk(uint32_t disp_id) {
  return 0;
}

Return<int32_t> HWCSession::setDSIClk(uint32_t disp_id, uint64_t bit_clk) {
  return 0;
}

Return<int32_t> HWCSession::setQsyncMode(uint32_t disp_id, IDisplayConfig::QsyncMode mode) {
  return 0;
}

Return<bool> HWCSession::isSmartPanelConfig(uint32_t disp_id, uint32_t config_id) {
  return false;
}

Return<bool> HWCSession::isAsyncVDSCreationSupported() {
  return false;
}

Return<int32_t> HWCSession::createVirtualDisplay(uint32_t width, uint32_t height, int32_t format) {
  return 0;
}

Return<bool> HWCSession::isRotatorSupportedFormat(int32_t format, bool ubwc) {
  return false;
}

Return<int32_t> HWCSession::registerQsyncCallback(const sp<IDisplayQsyncCallback>& callback) {
  return 0;
}

Return<int32_t> HWCSession::allowIdleFallback() {
  return 0;
}

Return<void> HWCSession::getFSCRGBOrder(DisplayType dpy,
                                        IDisplayConfig::getFSCRGBOrder_cb _hidl_cb) {
  return Void();
}

Return<int32_t> HWCSession::enableCAC(uint32_t disp_id, bool enable, float red, float green,
                                      float blue) {
  return 0;
}

Return<int32_t> HWCSession::setCacEyeConfig(uint32_t disp_id,
                  const IDisplayConfig::CacEyeConfig& left,
                  const IDisplayConfig::CacEyeConfig& right) {
  return 0;
}

Return<int32_t> HWCSession::setSkewVsync(uint32_t disp_id, uint32_t skew_vsync_val) {
  return 0;
}

Return<int32_t> HWCSession::tunnellingInit() {
  char property[PROPERTY_VALUE_MAX] = {0};
  property_get(ENABLE_TUNNELLING, property, "0");
  if (!(strncmp(property, "0", PROPERTY_VALUE_MAX))) {
     DLOGE("Tunnlling property not set. Exiting tunnellingInit!");
     return -EINVAL;
  }

  tunneling_enabled_ = true;
  return 0;
}

Return<int32_t> HWCSession::createTunnelledLayer(const IDisplayConfig::LayerInfo& layer) {
  if (tunneling_enabled_ == false) {
    DLOGE("Tunneling not enabled\n");
    return -EINVAL;
  }

  tunneled_layer_params_ = layer;
  return 0;
}

int32_t HWCSession::CreateTunneledLayerInternal() {
  int32_t error = -EINVAL;
  hwc2_device_t *device = static_cast<hwc2_device_t *>(this);
  error = CreateLayer(device, HWC_DISPLAY_PRIMARY, &tunneled_layer_);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("CreateLayer failed! Exiting createTunnelledLayer.");
    return error;
  }
  SetLayerIsTunneled(device, HWC_DISPLAY_PRIMARY, tunneled_layer_, true);
  error = SetLayerZOrder(device, HWC_DISPLAY_PRIMARY,tunneled_layer_,
                         tunneled_layer_params_.z_order);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerZOrder failed! Exiting createTunnelledLayer.");
    return error;
  }

  hwc_frect_t rect;
  rect.left = tunneled_layer_params_.src_rect.left;
  rect.top = tunneled_layer_params_.src_rect.top;
  rect.right = tunneled_layer_params_.src_rect.right;
  rect.bottom = tunneled_layer_params_.src_rect.bottom;
  error = SetLayerSourceCrop(device, HWC_DISPLAY_PRIMARY,
                             tunneled_layer_, rect);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerSourceCrop failed! Exiting createTunnelledLayer.");
    return error;
  }

  error = SetLayerTransform(device, HWC_DISPLAY_PRIMARY, tunneled_layer_,
                            (int32_t) tunneled_layer_params_.layer_transform);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerTransform failed! Exiting createTunnelledLayer.");
    return error;
  }

  hwc_rect_t dst_rect;
  dst_rect.left = tunneled_layer_params_.dst_rect.left;
  dst_rect.top = tunneled_layer_params_.dst_rect.top;
  dst_rect.right = tunneled_layer_params_.dst_rect.right;
  dst_rect.bottom = tunneled_layer_params_.dst_rect.bottom;
  error = SetLayerDisplayFrame(device, HWC_DISPLAY_PRIMARY, tunneled_layer_, dst_rect);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerDisplayFrame failed! Exiting createTunnelledLayer.");
    return error;
  }

  error = SetLayerPlaneAlpha(device, HWC_DISPLAY_PRIMARY, tunneled_layer_,
                             (float) tunneled_layer_params_.plane_alpha);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerPlaneAlpha failed! Exiting createTunnelledLayer.");
    return error;
  }

  error = SetLayerDataspace(device, HWC_DISPLAY_PRIMARY, tunneled_layer_,
                            tunneled_layer_params_.dataspace);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerDataspace failed! Exiting createTunnelledLayer.");
    return error;
  }

  error = SetLayerBlendMode(device, HWC_DISPLAY_PRIMARY, tunneled_layer_,
                            (int32_t) tunneled_layer_params_.blending);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerBlendMode failed! Exiting createTunnelledLayer.");
    return error;
  }

  return 0;
}

Return<void> HWCSession::dequeueTunnelledBuffer(const hidl_handle& buffer,
                                                dequeueTunnelledBuffer_cb _hidl_cb) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
  native_handle_t* handle = nullptr;
  if (tunneling_enabled_ == false) {
    DLOGE("Tunneling not enabled.");
    _hidl_cb(-EINVAL, handle);
    return Void();
  }

  DTRACE_SCOPED();

  const native_handle_t *native_handle = NULL;
  buffer_handle_t buffer_handle = buffer.getNativeHandle();
  if (!buffer_handle) {
    DLOGE("Invalid native handle");
    _hidl_cb(-EINVAL, handle);
    return Void();
  }

  uint64_t buffer_id = ((private_handle_t *)buffer_handle)->id;
  if (tunneling_map_buffer_native_handle_.find(buffer_id) !=
      tunneling_map_buffer_native_handle_.end()) {
    native_handle = tunneling_map_buffer_native_handle_[buffer_id];
  } else {
    native_handle = buffer_allocator_.ImportBuffer(buffer_handle);
    if (native_handle == nullptr) {
      _hidl_cb(-EINVAL, handle);
      return Void();
    }
    tunneling_map_buffer_native_handle_[((private_handle_t *)native_handle)->id] = native_handle;
  }
  private_handle_t *private_handle = (private_handle_t *)native_handle;
  if(private_handle == nullptr) {
    _hidl_cb(-EINVAL, handle);
    return Void();;
  }

  if (tunneling_map_buffer_release_fence_.find(private_handle->id) ==
      tunneling_map_buffer_release_fence_.end()) {
    _hidl_cb(-EINVAL, handle);
    return Void();
  }

  int32_t release_fence = tunneling_map_buffer_release_fence_[private_handle->id];

  NATIVE_HANDLE_DECLARE_STORAGE(fenceStorage, 1, 0);
  if (release_fence >= 0) {
    tunneled_layer_rf_ = release_fence;
    handle = native_handle_init(fenceStorage, 1, 0);
    if (handle) {
     handle->data[0] = release_fence;
    }
  }

  DLOGV("dequeueTunnelledBuffer successful.");

  _hidl_cb(0, handle);
  return Void();
}

Return<int32_t> HWCSession::queueTunnelledBuffer(const hidl_handle& buffer,
                                                 const hidl_handle& acquire_fence) {
  if (tunneling_enabled_ == false) {
    DLOGW("Tunneling not enabled.");
    return -EINVAL;
  }

  int32_t error = -EINVAL;
  if(tunneled_layer_ == -1) {
    error = CreateTunneledLayerInternal();
    if (error != HWC2_ERROR_NONE) {
      return -EINVAL;
    }
  }

  DTRACE_SCOPED();

  bool tunneled_layer_present = false;
  IsTunnelledLayerPresent(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY,
                          &tunneled_layer_present);
  if (tunneled_layer_present == false || tunneled_layer_ == -1) {
    tunneled_layer_ = -1;
    DLOGW("No tunneled layer present! Exiting queueTunnelledBuffer");
    return -EINVAL;
  }

  const native_handle_t *native_handle = NULL;
  buffer_handle_t buffer_handle = buffer.getNativeHandle();
  if (!buffer_handle) {
    DLOGE("Invalid native handle.");
    return -EINVAL;
  }

  uint64_t buffer_id = ((private_handle_t *)buffer_handle)->id;
  if (tunneling_map_buffer_native_handle_.find(buffer_id) !=
      tunneling_map_buffer_native_handle_.end()) {
    native_handle = tunneling_map_buffer_native_handle_[buffer_id];
  } else {
    native_handle = buffer_allocator_.ImportBuffer(buffer_handle);
    if (native_handle == nullptr) {
      return -EINVAL;
    }
    tunneling_map_buffer_native_handle_[((private_handle_t *)native_handle)->id] = native_handle;
  }

  uint32_t types_count = 0;
  uint32_t reqs_count = 0;
  HWCDisplay *hwc_display = hwc_display_[HWC_DISPLAY_PRIMARY];
  const native_handle_t* native_fence_handle = acquire_fence.getNativeHandle();
  int acquire_fence_fd = -1;
  // if native_fence_handle is NULL, acquire fence fd is considered -1
  if (native_fence_handle) {
     acquire_fence_fd = dup(native_fence_handle->data[0]);
  }
  {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);
  }
  error = SetLayerBuffer(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY, tunneled_layer_,
                         native_handle, acquire_fence_fd);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("SetLayerBuffer failed! Exiting queueTunnelledBuffer.");
    return error;
  }

  if (hwc_display->IsSkipValidateState() && !hwc_display->CanSkipValidate()) {
    error = ValidateDisplay(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY, &types_count, &reqs_count);
    if (error != HWC2_ERROR_NONE && error != HWC2_ERROR_HAS_CHANGES) {
      DLOGE("ValidateDisplay failed! Exiting queueTunnelledBuffer.");
      return error;
    }
  }

  int presentfence = 0;
  error = PresentDisplay(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY, &presentfence);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("PresentDisplay failed! Exiting queueTunnelledBuffer.");
    return error;
  }
  close(presentfence);

  auto hwc_layer = hwc_display->GetHWCLayer(tunneled_layer_);
  if (hwc_layer == nullptr) {
    DLOGE("Unable to fetch corresponding hwc_layer for tunneled layer.");
    return -EINVAL;
  }
  int release_fence = hwc_layer->PopBackReleaseFence();
  close(tunneled_layer_rf_);
  tunneling_map_buffer_release_fence_[((private_handle_t *)native_handle)->id] = release_fence;

  DLOGV("queueTunnelledBuffer successful.");
  return 0;
}

Return<int32_t> HWCSession::destroyTunnelledLayer()  {
  if (tunneling_enabled_ == false) {
    DLOGE("Tunneling not enabled");
    return -EINVAL;
  }
  DTRACE_SCOPED();

  bool tunneled_layer_present = false;
  IsTunnelledLayerPresent(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY,
                          &tunneled_layer_present);
  if (tunneled_layer_present == false || tunneled_layer_ == -1) {
    tunneled_layer_ = -1;
    DLOGW("No tunneled layer present! Exiting destroyTunnelledLayer");
    return -EINVAL;
  }

  SetLayerIsTunneled(static_cast<hwc2_device_t *>(this),
                     HWC_DISPLAY_PRIMARY, tunneled_layer_, false);

  int error = DestroyLayer(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY,
                           tunneled_layer_);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("DestroyLayer failed, tunneled layer not found!");
  }

  tunneled_layer_ = -1;

  uint32_t types_count = 0;
  uint32_t reqs_count = 0;

  error = ValidateDisplay(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY, &types_count, &reqs_count);

  if (error != HWC2_ERROR_NONE && error != HWC2_ERROR_HAS_CHANGES) {
    DLOGE("ValidateDisplay failed! Exiting destroyTunnelledLayer.");
    return error;
  }

  int presentfence = 0;
  error = PresentDisplay(static_cast<hwc2_device_t *>(this), HWC_DISPLAY_PRIMARY, &presentfence);
  if (error != HWC2_ERROR_NONE) {
    DLOGE("PresentDisplay failed! Exiting destroyTunnelledLayer.");
    return error;
  }
  return 0;
}

Return<int32_t> HWCSession::tunnellingDeinit() {
  tunneling_enabled_ = false;
  for (auto i : tunneling_map_buffer_native_handle_) {
    native_handle_close(i.second);
  }
  for (auto i : tunneling_map_buffer_release_fence_) {
    close(i.second);
  }
  tunneling_map_buffer_native_handle_.clear();
  tunneling_map_buffer_release_fence_.clear();
  return 0;
}
#endif // DISPLAY_CONFIG_1_21

}  // namespace sdm
