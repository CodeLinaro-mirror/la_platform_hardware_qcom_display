/*
* Copyright (c) 2021 The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
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
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <QtiGralloc.h>

#include "DisplayConfigAIDL.h"

#include "sdm_interface_factory.h"
#include "display_properties.h"

using sdm::SDMInterfaceFactory;

using ::aidl::android::hardware::common::NativeHandle;
using sdm::Locker;

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace config {

DisplayConfigAIDL::DisplayConfigAIDL() {
  SDMInterfaceFactory *sdm_factory = nullptr;
  sdm_factory = sdm::GetSDMInterfaceFactory();

  caps_ = sdm_factory->CreateCapsIntf();
  settings_ = sdm_factory->CreateSettingsIntf();
  lifecycle_ = sdm_factory->CreateLifeCycleIntf();
  lifecycle_->RegisterSideBandCallback(this);
  drawcycle_ = sdm_factory->CreateDrawCycleIntf();
  sideband_ = sdm_factory->CreateSideBandIntf();
  layer_builder_ = sdm_factory->CreateLayerBuilderIntf();
}

int MapDisplayType(DisplayType dpy) {
  switch (dpy) {
    case DisplayType::PRIMARY:
      return qdutils::DISPLAY_PRIMARY;

    case DisplayType::EXTERNAL:
      return qdutils::DISPLAY_EXTERNAL;

    case DisplayType::VIRTUAL:
      return qdutils::DISPLAY_VIRTUAL;

    case DisplayType::BUILTIN2:
      return qdutils::DISPLAY_BUILTIN_2;

    default:
      break;
  }

  return -EINVAL;
}

sdm::SDMDisplayStatus MapExternalStatus(ExternalStatus status) {
  switch (status) {
    case ExternalStatus::OFFLINE:
      return sdm::SDMDisplayStatus::kDisplayStatusOffline;

    case ExternalStatus::ONLINE:
      return sdm::SDMDisplayStatus::kDisplayStatusOnline;

    case ExternalStatus::PAUSE:
      return sdm::SDMDisplayStatus::kDisplayStatusPause;

    case ExternalStatus::RESUME:
      return sdm::SDMDisplayStatus::kDisplayStatusResume;

    default:
      break;
  }

  return sdm::SDMDisplayStatus::kDisplayStatusInvalid;
}

bool WaitForResourceNeeded(sdm::SDMPowerMode prev_mode, sdm::SDMPowerMode new_mode) {
  return ((prev_mode == sdm::SDMPowerMode::POWER_MODE_OFF) &&
          (new_mode == sdm::SDMPowerMode::POWER_MODE_ON ||
           new_mode == sdm::SDMPowerMode::POWER_MODE_DOZE));
}

ScopedAStatus DisplayConfigAIDL::isDisplayConnected(DisplayType dpy, bool *connected) {
  int disp_id = MapDisplayType(dpy);

  *connected = lifecycle_->IsDisplayConnected(disp_id);

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setDisplayStatus(DisplayType dpy, ExternalStatus status) {
  int disp_id = MapDisplayType(dpy);
  sdm::SDMDisplayStatus external_status = MapExternalStatus(status);

  if (lifecycle_->SetDisplayStatus(disp_id, external_status) != 0) {
    ALOGW("%s: Setting status:%d to display:%d failed", __FUNCTION__, status, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::configureDynRefreshRate(DynRefreshRateOp op, int refresh_rate) {
  sdm::SDMBuiltInDisplayOps ops;
  int val = 0;

  switch (op) {
    case DynRefreshRateOp::DISABLE_METADATA:
      ops = sdm::SDMBuiltInDisplayOps::SET_METADATA_DYN_REFRESH_RATE;
      val = false;
      break;

    case DynRefreshRateOp::ENABLE_METADATA:
      ops = sdm::SDMBuiltInDisplayOps::SET_METADATA_DYN_REFRESH_RATE;
      val = true;
      break;

    case DynRefreshRateOp::SET_BINDER:
      ops = sdm::SDMBuiltInDisplayOps::SET_BINDER_DYN_REFRESH_RATE;
      val = refresh_rate;
      break;

    default:
      ALOGW("%s: Invalid operation %d", __FUNCTION__, op);
      return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  auto ret = settings_->ConfigureDynRefreshRate(ops, val);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: Display = %d is not connected.", __FUNCTION__, sdm::HWC_DISPLAY_PRIMARY);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getConfigCount(DisplayType dpy, int *count) {
  auto error = lifecycle_->GetConfigCount(MapDisplayType(dpy), (uint32_t *)count);
  if (error != sdm::kErrorNone) {
    ALOGW("%s: Failed to retrieve config count for display:%d", __FUNCTION__, dpy);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getActiveConfig(DisplayType dpy, int *config) {
  int disp_id = MapDisplayType(dpy);

  int error = drawcycle_->GetActiveConfigIndex(disp_id, (uint32_t *)config);
  if (error != sdm::kErrorNone) {
    ALOGW("%s: Failed to retrieve the active config index for display:%d", __FUNCTION__, dpy);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setActiveConfig(DisplayType dpy, int config) {
  int disp_id = MapDisplayType(dpy);

  if (drawcycle_->SetActiveConfigIndex(disp_id, (uint32_t)config) != sdm::kErrorNone) {
    ALOGW("%s: Failed to set active config index to display:%d", __FUNCTION__, dpy);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getDisplayAttributes(int config_index, DisplayType dpy,
                                                      Attributes *attributes) {
  auto error = sdm::kErrorNone;
  int disp_id = MapDisplayType(dpy);

  // TODO (aparmar): seq lk
  sdm::DisplayConfigVariableInfo var_info{};
  uint32_t group_id = -1;
  error = settings_->GetDisplayAttributes(disp_id, config_index, &var_info, &group_id);

  if (error != sdm::kErrorNone) {
    ALOGW("%s: Invalid display = %d", __FUNCTION__, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  attributes->vsyncPeriod = var_info.vsync_period_ns;
  attributes->xRes = var_info.x_pixels;
  attributes->yRes = var_info.y_pixels;
  attributes->xDpi = var_info.x_dpi;
  attributes->yDpi = var_info.y_dpi;
  attributes->panelType = DisplayPortType::DEFAULT;
  attributes->isYuv = var_info.is_yuv;

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setPanelBrightness(int level) {
  if (!(0 <= level && level <= 255)) {
    ALOGW("%s: Invalid panel brightness level :%d", __FUNCTION__, level);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  if (level == 0) {
    settings_->SetDisplayBrightness(sdm::HWC_DISPLAY_PRIMARY, -1.0f);
  } else {
    settings_->SetDisplayBrightness(sdm::HWC_DISPLAY_PRIMARY, (level - 1) / 254.0f);
  }
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getPanelBrightness(int *level) {
  float brightness = -1.0f;

  settings_->GetDisplayBrightness(sdm::HWC_DISPLAY_PRIMARY, &brightness);
  if (brightness == -1.0f) {
    *level = 0;
  } else {
    *level = static_cast<uint32_t>(254.0f * brightness + 1);
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::minHdcpEncryptionLevelChanged(DisplayType dpy, int min_enc_level) {
  drawcycle_->MinHdcpEncryptionLevelChanged(MapDisplayType(dpy), min_enc_level);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::refreshScreen() {
  // TODO(aparmar): seq lk
  drawcycle_->Refresh(sdm::HWC_DISPLAY_PRIMARY);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::controlPartialUpdate(DisplayType dpy, bool enable) {
  settings_->ControlPartialUpdate(MapDisplayType(dpy), enable);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::toggleScreenUpdate(bool on) {
  settings_->ToggleScreenUpdate(on);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setIdleTimeout(int value) {
  settings_->SetIdleTimeout(value);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getHDRCapabilities(DisplayType dpy, HDRCapsParams *caps) {
  int error = -EINVAL;

  do {
    int disp_id = MapDisplayType(dpy);

    // query number of hdr types
    uint32_t out_num_types = 0;
    float out_max_luminance = 0.0f;
    float out_max_average_luminance = 0.0f;
    float out_min_luminance = 0.0f;
    if (caps_->GetHdrCapabilities(disp_id, &out_num_types, nullptr, &out_max_luminance,
                                  &out_max_average_luminance,
                                  &out_min_luminance) != sdm::kErrorNone) {
      break;
    }
    if (!out_num_types) {
      error = 0;
      break;
    }

    // query hdr caps
    caps->supportedHdrTypes.resize(out_num_types);

    if (caps_->GetHdrCapabilities(disp_id, &out_num_types, caps->supportedHdrTypes.data(),
                                  &out_max_luminance, &out_max_average_luminance,
                                  &out_min_luminance) == sdm::kErrorNone) {
      error = 0;
    }
  } while (false);

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setCameraLaunchStatus(int on) {
  sideband_->SetCameraLaunchStatus(on);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::displayBWTransactionPending(bool *status) {
  sideband_->DisplayBWTransactionPending(status);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setDisplayAnimating(long display_id, bool animating) {
  sideband_->SetDisplayAnimating(display_id, animating);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::controlIdlePowerCollapse(bool enable, bool synchronous) {
  sideband_->ControlIdlePowerCollapse(enable, synchronous);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getWriteBackCapabilities(bool *is_wb_ubwc_supported) {
  caps_->IsWbUbwcSupported(is_wb_ubwc_supported);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setDisplayDppsAdROI(int display_id, int h_start, int h_end,
                                                     int v_start, int v_end, int factor_in,
                                                     int factor_out) {
  settings_->SetDisplayDppsAdROI(display_id, h_start, h_end, v_start, v_end, factor_in, factor_out);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::updateVSyncSourceOnPowerModeOff() {
  settings_->UpdateVSyncSourceOnPowerModeOff();
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::updateVSyncSourceOnPowerModeDoze() {
  settings_->UpdateVSyncSourceOnPowerModeDoze();
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setPowerMode(int disp_id, PowerMode power_mode) {
  // This API is deprecated
  return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ScopedAStatus DisplayConfigAIDL::isPowerModeOverrideSupported(int disp_id, bool *supported) {
  *supported = false;
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isHDRSupported(int disp_id, bool *supported) {
  if (disp_id < 0 || disp_id >= sdm::kNumDisplays) {
    ALOGW("%s: Not valid display", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  *supported = settings_->IsHDRDisplay(disp_id);

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isWCGSupported(int disp_id, bool *supported) {
  isHDRSupported(disp_id, supported);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setLayerAsMask(int disp_id, long layer_id) {
  auto err = layer_builder_->SetLayerAsMask(disp_id, layer_id);
  if (err != sdm::kErrorNone) {
    return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getDebugProperty(const std::string &prop_name,
                                                  std::string *value) {
  std::string vendor_prop_name = DISP_PROP_PREFIX;
  int error = -EINVAL;
  char val[64] = {};

  vendor_prop_name += prop_name.c_str();
  if (!sideband_->GetProperty(vendor_prop_name.c_str(), val)) {
    *value = val;
    error = 0;
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setClientUp() {
  sideband_->SetClientUp();

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getActiveBuiltinDisplayAttributes(Attributes *attr) {
  uint64_t disp_id = sdm::kNumDisplays;
  auto error = settings_->GetActiveBuiltinDisplay(&disp_id);
  if (error != sdm::kErrorNone) {
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  sdm::Config config = -1;
  error = settings_->GetActiveConfig(disp_id, &config);
  if (error != sdm::kErrorNone) {
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  sdm::DisplayConfigVariableInfo var_info{};
  uint32_t group_id = -1;
  error = settings_->GetDisplayAttributes(disp_id, config, &var_info, &group_id);

  if (error != sdm::kErrorNone) {
    ALOGW("%s: Invalid display = %d", __FUNCTION__, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  attr->vsyncPeriod = var_info.vsync_period_ns;
  attr->xRes = var_info.x_pixels;
  attr->yRes = var_info.y_pixels;
  attr->xDpi = var_info.x_dpi;
  attr->yDpi = var_info.y_dpi;
  attr->panelType = DisplayPortType::DEFAULT;
  attr->isYuv = var_info.is_yuv;

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setPanelLuminanceAttributes(int disp_id, float min_lum,
                                                             float max_lum) {
  // currently doing only for virtual display
  if (disp_id != static_cast<int>(DisplayType::VIRTUAL)) {
    ALOGW("%s: Setting panel luminance on non virtual display is not supported", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  // check for out of range luminance values
  if (min_lum <= 0.0f || min_lum >= 1.0f || max_lum <= 100.0f || max_lum >= 1000.0f) {
    ALOGW("%s: Luminance values are out of range : minimum_luminance:%f maximum_luminance:%f",
          __FUNCTION__, min_lum, max_lum);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  settings_->SetPanelLuminanceAttributes(disp_id, min_lum, max_lum);

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isBuiltInDisplay(int disp_id, bool *is_built_in) {
  *is_built_in = sideband_->IsBuiltInDisplay(disp_id);

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isAsyncVDSCreationSupported(bool *supported) {
  *supported = sideband_->IsAsyncVDSCreationSupported();

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::createVirtualDisplay(int width, int height, int format) {
  auto ret = sideband_->CreateVirtualDisplay(width, height, format);
  if (ret != sdm::kErrorNone) {
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getSupportedDSIBitClks(int disp_id, std::vector<long> *bit_clks) {
  auto ret = caps_->GetSupportedDSIClock(disp_id, bit_clks);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: Display: %d is not connected", __FUNCTION__, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getDSIClk(int disp_id, long *bit_clk) {
  auto ret = settings_->GetDSIClk(disp_id, (uint64_t *)bit_clk);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: Invalid display: %d", __FUNCTION__, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setDSIClk(int disp_id, long bit_clk) {
  auto ret = settings_->SetDSIClk(disp_id, (uint64_t)bit_clk);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: Invalid display: %d", __FUNCTION__, disp_id);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setQsyncMode(int disp_id, QsyncMode mode) {
  sdm::QSyncMode qsync_mode = sdm::kQSyncModeNone;
  switch (mode) {
    case QsyncMode::NONE:
      qsync_mode = sdm::kQSyncModeNone;
      break;

    case QsyncMode::WAIT_FOR_FENCES_ONE_FRAME:
      qsync_mode = sdm::kQsyncModeOneShot;
      break;

    case QsyncMode::WAIT_FOR_FENCES_EACH_FRAME:
      qsync_mode = sdm::kQsyncModeOneShotContinuous;
      break;

    case QsyncMode::WAIT_FOR_COMMIT_EACH_FRAME:
      qsync_mode = sdm::kQSyncModeContinuous;
      break;
  }

  auto ret = settings_->SetQsyncMode(disp_id, qsync_mode);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: failed: %d", __FUNCTION__, ret);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isSmartPanelConfig(int disp_id, int config_id, bool *is_smart) {
  auto ret = caps_->IsSmartPanelConfig(disp_id, config_id, is_smart);
  if (ret != sdm::kErrorNone) {
    ALOGW("%s: failed: %d", __FUNCTION__, ret);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isRotatorSupportedFormat(int hal_format, bool ubwc,
                                                          bool *supported) {
  int flag = ubwc ? qtigralloc::PRIV_FLAGS_UBWC_ALIGNED : 0;
  sdm::LayerBufferFormat sdm_format = layer_builder_->GetSDMFormat(hal_format, flag, 0);

  *supported = caps_->IsRotatorSupportedFormat(sdm_format);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::controlQsyncCallback(bool enable) {
  if (enable) {
    qsync_callback_ = callback_;
  } else {
    qsync_callback_.reset();
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::sendTUIEvent(DisplayType dpy, TUIEventType event_type) {
  int disp_id = MapDisplayType(dpy);
  auto ret = sideband_->TUIEventHandler(disp_id, static_cast<sdm::SDMTUIEventType>(event_type));
  if (ret != sdm::kErrorNone) {
    ALOGW("TUIEventHandler failed with %d", ret);
    return ScopedAStatus(AStatus_fromServiceSpecificError(EX_ILLEGAL_ARGUMENT));
  }
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getDisplayHwId(int disp_id, int *display_hw_id) {
  auto ret = caps_->GetDisplayHwId(disp_id, display_hw_id);
  if (ret != sdm::kErrorNone) {
    ALOGW("getDisplayHwId failed with %d", ret);
    return ScopedAStatus(AStatus_fromServiceSpecificError(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::getSupportedDisplayRefreshRates(
    DisplayType dpy, std::vector<int> *supported_refresh_rates) {
  caps_->GetSupportedDisplayRefreshRates(MapDisplayType(dpy),
                                         (std::vector<uint32_t> *)supported_refresh_rates);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isRCSupported(int disp_id, bool *supported) {
  // Mask layers can potentially be shown on any display so report RC supported on all displays if
  // the property enables the feature for use.
  int val = false;  // Default value.
  sideband_->GetProperty(ENABLE_ROUNDED_CORNER, &val);
  *supported = val ? true : false;

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::controlIdleStatusCallback(bool enable) {
  if (enable) {
    idle_callback_ = callback_;
  } else {
    idle_callback_.reset();
  }

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::isSupportedConfigSwitch(int disp_id, int config, bool *supported) {
  *supported = caps_->IsModeSwitchAllowed(disp_id, config);
  return ScopedAStatus::ok();
}

DisplayType GetDisplayConfigDisplayType(int qdutils_disp_type) {
  switch (qdutils_disp_type) {
    case qdutils::DISPLAY_PRIMARY:
      return DisplayType::PRIMARY;

    case qdutils::DISPLAY_EXTERNAL:
      return DisplayType::EXTERNAL;

    case qdutils::DISPLAY_VIRTUAL:
      return DisplayType::VIRTUAL;

    case qdutils::DISPLAY_BUILTIN_2:
      return DisplayType::BUILTIN2;

    default:
      return DisplayType::INVALID;
  }
}

int DisplayConfigAIDL::GetDispTypeFromPhysicalId(uint64_t physical_disp_id,
                                                 DisplayType *disp_type) {
  // TODO(user): Least significant 8 bit is port id based on the SF current implementaion. Need to
  // revisit this if there is a change in logic to create physical display id in SF.
  int port_id = (physical_disp_id & 0xFF);
  int out_port = 0;
  for (int dpy = qdutils::DISPLAY_PRIMARY; dpy <= qdutils::DISPLAY_EXTERNAL_2; dpy++) {
    auto ret = caps_->GetDisplayPortId(dpy, &out_port);
    if (ret != sdm::kErrorNone) {
      return ret;
    }
    if (port_id == out_port) {
      *disp_type = GetDisplayConfigDisplayType(dpy);
      return 0;
    }
  }

  return -ENODEV;
}

ScopedAStatus DisplayConfigAIDL::getDisplayType(long physical_disp_id, DisplayType *display_type) {
  if (!display_type) {
    ALOGW("%s: Display type provided is invalid.", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  GetDispTypeFromPhysicalId(physical_disp_id, display_type);
  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::setCWBOutputBuffer(
    const std::shared_ptr<IDisplayConfigCallback> &callback, int32_t disp_id, const Rect &rect,
    bool post_processed, const NativeHandle &buffer) {
  if (!callback) {
    ALOGE("%s: Callback provided is invalid.", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  std::unordered_map<int32_t, int32_t> disp_type_map = {
      {static_cast<int32_t>(DisplayType::PRIMARY), static_cast<int32_t>(qdutils::DISPLAY_PRIMARY)},
      {static_cast<int32_t>(DisplayType::EXTERNAL),
       static_cast<int32_t>(qdutils::DISPLAY_EXTERNAL)},
      {static_cast<int32_t>(DisplayType::BUILTIN2),
       static_cast<int32_t>(qdutils::DISPLAY_BUILTIN_2)},
  };

  if (disp_id <= static_cast<int32_t>(DisplayType::INVALID) ||
      disp_id > static_cast<int32_t>(DisplayType::BUILTIN2)) {
    ALOGE("%s: CWB is supported on primary or external display only at present.", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  int32_t display_type = disp_type_map[disp_id];

  sdm::CwbConfig cwb_config = {};
  cwb_config.tap_point = static_cast<sdm::CwbTapPoint>(post_processed);
  sdm::LayerRect &roi = cwb_config.cwb_roi;
  roi.left = FLOAT(rect.left);
  roi.top = FLOAT(rect.top);
  roi.right = FLOAT(rect.right);
  roi.bottom = FLOAT(rect.bottom);

  ALOGI("CWB config passed by cwb_client : tappoint %d  CWB_ROI : (%f %f %f %f)",
        cwb_config.tap_point, roi.left, roi.top, roi.right, roi.bottom);

  auto ret_status = EX_NONE;

  void *hdl = sdm::ConvertToSnapHandle(buffer);

  if (!handle_importer_.importBuffer(static_cast<const SnapHandle *>(hdl))) {
    ALOGE("%s: Snapmapper retain failed.", __FUNCTION__);
    ret_status = EX_ILLEGAL_ARGUMENT;
  }

  if (ret_status == EX_NONE && cwb_callbacks_.find(hdl) != cwb_callbacks_.end()) {
    ALOGE("%s: buffer already being handled", __FUNCTION__);
    ret_status = EX_ILLEGAL_ARGUMENT;
  }

  if (ret_status == EX_NONE) {
    sdm::DisplayError ret = sideband_->PostBuffer(cwb_config, hdl, display_type);
    if (ret != sdm::kErrorNone) {
      ret_status = EX_TRANSACTION_FAILED;
    } else {
      cwb_callbacks_.insert({hdl, callback});
    }
  }

  if (ret_status != EX_NONE) {
    // Need to unregister and delete snap handle on CWB request rejection/failure
    handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
  }

  return (ret_status == EX_NONE) ? ScopedAStatus::ok()
                                 : ScopedAStatus(AStatus_fromExceptionCode(ret_status));
}

void DisplayConfigAIDL::NotifyCWBStatus(int32_t status, void *hdl) {
  if (cwb_callbacks_.find(hdl) == cwb_callbacks_.end()) {
    ALOGE("%s: buffer not found", __FUNCTION__);
    return;
  }

  std::shared_ptr<IDisplayConfigCallback> callback = cwb_callbacks_[hdl];

  NativeHandle buffer =
      sdm::AIDLNativeHandleFromSnapHandle(reinterpret_cast<SnapHandle *>(hdl), false);
  if (callback) {
    ALOGI("Notify the client about buffer status %d.", status);

    callback->notifyCWBBufferDone(status, buffer);
  }

  cwb_callbacks_.erase(hdl);
  handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
}

ScopedAStatus DisplayConfigAIDL::setCameraSmoothInfo(CameraSmoothOp op, int32_t fps) {
  int ret = -1;

  if (fps < 0) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  ret = sideband_->SetCameraSmoothInfo(static_cast<sdm::SDMCameraSmoothOp>(op), fps);

  return ret == sdm::kErrorNone ? ScopedAStatus::ok()
                                : ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
}

ScopedAStatus DisplayConfigAIDL::registerCallback(
    const std::shared_ptr<IDisplayConfigCallback> &callback, int64_t *client_handle) {
  int ret = -1;

  if (callback == nullptr) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  callback_clients_.emplace(callback_client_id_, callback);
  *client_handle = callback_client_id_;
  callback_client_id_++;

  return ScopedAStatus::ok();
}

ScopedAStatus DisplayConfigAIDL::unRegisterCallback(int64_t client_handle) {
  int ret = -1;
  bool removed = false;

  if (client_handle < 0) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  for (auto it = callback_clients_.begin(); it != callback_clients_.end();) {
    if (it->first == client_handle) {
      it = callback_clients_.erase(it);
      removed = true;
    } else {
      it++;
    }
  }

  return removed ? ScopedAStatus::ok() : ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
}

ScopedAStatus DisplayConfigAIDL::getDisplayPortId(int32_t disp_id, int32_t *port_id) {
  int ret = -1;

  if (!port_id) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  ret = caps_->GetDisplayPortId(disp_id, port_id);

  return ret == 0 ? ScopedAStatus::ok() : ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
}

void DisplayConfigAIDL::NotifyQsyncChange(uint64_t display_id, bool qsync_enabled,
                                          uint32_t refresh_rate, uint32_t qsync_refresh_rate) {
  // AIDL callback
  if (!callback_clients_.empty()) {
    std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
    for (auto const &[id, callback] : callback_clients_) {
      if (callback) {
        callback->notifyQsyncChange(qsync_enabled, refresh_rate, qsync_refresh_rate);
      }
    }
  }

  // HIDL callback
  std::shared_ptr<DisplayConfig::ConfigCallback> callback = qsync_callback_.lock();
  if (!callback) {
    return;
  }

  callback->NotifyQsyncChange(qsync_enabled, refresh_rate, qsync_refresh_rate);
}

void DisplayConfigAIDL::NotifyCameraSmoothInfo(sdm::SDMCameraSmoothOp op, int32_t fps) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);

  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyCameraSmoothInfo(static_cast<CameraSmoothOp>(op), fps);
    }
  }
}

void DisplayConfigAIDL::NotifyResolutionChange(uint64_t display_id,
                                               sdm::SDMConfigAttributes &attr) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);

  Attributes attributes{};
  attributes.vsyncPeriod = attr.vsyncPeriod;
  attributes.xRes = attr.xRes;
  attributes.yRes = attr.yRes;
  attributes.xDpi = attr.xDpi;
  attributes.yDpi = attr.yDpi;
  attributes.panelType = static_cast<DisplayPortType>(attr.panelType);

  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyResolutionChange(display_id, attributes);
    }
  }
}

typedef ::aidl::vendor::qti::hardware::display::config::DisplayType AIDLDisplayType;
AIDLDisplayType MapDisplayId(int disp_id) {
  switch (disp_id) {
    case qdutils::DISPLAY_PRIMARY:
      return AIDLDisplayType::PRIMARY;

    case qdutils::DISPLAY_EXTERNAL:
      return AIDLDisplayType::EXTERNAL;

    case qdutils::DISPLAY_VIRTUAL:
      return AIDLDisplayType::VIRTUAL;

    case qdutils::DISPLAY_BUILTIN_2:
      return AIDLDisplayType::BUILTIN2;

    default:
      break;
  }

  return AIDLDisplayType::INVALID;
}

void DisplayConfigAIDL::NotifyTUIEventDone(uint32_t ret, uint32_t disp_id,
                                           sdm::SDMTUIEventType event_type) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);

  AIDLDisplayType disp_type = MapDisplayId(disp_id);
  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyTUIEventDone(ret, disp_type, static_cast<TUIEventType>(event_type));
    }
  }
}

void DisplayConfigAIDL::NotifyIdleStatus(bool status) {
  std::shared_ptr<DisplayConfig::ConfigCallback> callback = idle_callback_.lock();
  if (!callback) {
    return;
  }

  callback->NotifyIdleStatus(true);
}

void DisplayConfigAIDL::OnHdmiHotplug(bool connected) {
  // TODO(aparmar)
}

}  // namespace config
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
