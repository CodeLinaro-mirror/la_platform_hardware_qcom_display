/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <QtiGralloc.h>
#include <android/binder_manager.h>
#include <utils/Timers.h>

#include "AmbientDataCaptureAIDL.h"

#include "sdm_interface_factory.h"
#include "display_properties.h"

using ::aidl::android::hardware::common::NativeHandle;
using AlgoConfigType =
    aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCAlgoConfigs::AlgoConfigType;
using sdm::SDMInterfaceFactory;

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace qacs {
namespace ambientdatacapture {

AmbientDataCaptureAIDL::AmbientDataCaptureAIDL() {
  SDMInterfaceFactory *sdm_factory = sdm::GetSDMInterfaceFactory();
  if (!sdm_factory) {
    ALOGE("%s: Failed to get SDM interface factory", __FUNCTION__);
    return;
  }

  settings_ = sdm_factory->CreateSettingsIntf();
  lifecycle_ = sdm_factory->CreateLifeCycleIntf();
  if (lifecycle_) {
    lifecycle_->RegisterSideBandCallback(this, true);
  }
  drawcycle_ =
      reinterpret_pointer_cast<SDMDisplayDrawCycleIntfV>(sdm_factory->CreateDrawCycleIntf());
  sideband_ = sdm_factory->CreateSideBandIntf();
}

AmbientDataCaptureAIDL::~AmbientDataCaptureAIDL() {
  if (lifecycle_) {
    lifecycle_->RegisterSideBandCallback(this, false);
  }
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

ScopedAStatus AmbientDataCaptureAIDL::getActiveConfig(DisplayType dpy, int32_t *config) {
  if (!config) {
    ALOGE("%s: config parameter is null", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  int disp_id = MapDisplayType(dpy);
  if (disp_id < 0) {
    ALOGE("%s: Invalid display type: %d", __FUNCTION__, static_cast<int>(dpy));
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  int error = drawcycle_->GetActiveConfigIndex(disp_id, reinterpret_cast<uint32_t *>(config));
  if (error != sdm::kErrorNone) {
    ALOGW("%s: Failed to retrieve the active config index for display:%d", __FUNCTION__, dpy);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  return ScopedAStatus::ok();
}

ScopedAStatus AmbientDataCaptureAIDL::getDisplayAttributes(int32_t config_index, DisplayType dpy,
                                                           Attributes *attributes) {
  if (!attributes) {
    ALOGE("%s: attributes parameter is null", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  auto error = sdm::kErrorNone;
  int disp_id = MapDisplayType(dpy);

  sdm::DisplayConfigVariableInfo var_info{};
  error = settings_->GetDisplayAttributes(disp_id, config_index, &var_info);

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

ScopedAStatus AmbientDataCaptureAIDL::setAlgoConfig(const ADCAlgoConfigs &algoConfigs) {
  bool success = true;
  switch (algoConfigs.algoConfigType) {
    case AlgoConfigType::SMART_SEL:
      success = true;
      break;
    default:
      success = false;
      break;
  }
  return (success ? ScopedAStatus::ok()
                  : ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT)));
}

ScopedAStatus AmbientDataCaptureAIDL::captureOutputBuffer(
    const std::shared_ptr<IAmbientDataCaptureCallback> &callback,
    const ADCDisplayConfigs &captureConfigs) {
  if (!callback) {
    ALOGW("%s: Callback provided is invalid.", __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

#define DISPLAY_TYPE_EXTERNAL_2 (UINT32(DisplayType::BUILTIN2) + 1)
  // Map display ID to display type
  std::unordered_map<int32_t, int32_t> disp_type_map = {
      {static_cast<int32_t>(DisplayType::PRIMARY), static_cast<int32_t>(qdutils::DISPLAY_PRIMARY)},
      {static_cast<int32_t>(DisplayType::EXTERNAL),
       static_cast<int32_t>(qdutils::DISPLAY_EXTERNAL)},
      {static_cast<int32_t>(DisplayType::BUILTIN2),
       static_cast<int32_t>(qdutils::DISPLAY_BUILTIN_2)},
      {static_cast<int32_t>(DISPLAY_TYPE_EXTERNAL_2),
       static_cast<int32_t>(qdutils::DISPLAY_EXTERNAL_2)},
  };

  if (captureConfigs.dispID <= static_cast<int32_t>(DisplayType::INVALID) ||
      captureConfigs.dispID > static_cast<int32_t>(DISPLAY_TYPE_EXTERNAL_2)) {
    ALOGE("%s: Invalid display ID: %d", __FUNCTION__, captureConfigs.dispID);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }

  auto it = disp_type_map.find(captureConfigs.dispID);
  if (it == disp_type_map.end()) {
    ALOGE("%s: Display ID %d not found in mapping", __FUNCTION__, captureConfigs.dispID);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }
  int32_t display_type = it->second;
  auto ret_status = EX_NONE;
  void *hdl = sdm::ConvertToSnapHandle(captureConfigs.buffer);
  bool hdl_imported = false;
  if (!hdl || !handle_importer_.importBuffer(static_cast<const SnapHandle *>(hdl))) {
    ALOGE("%s: Either retrieving snaphandle or importing buffer failed.", __FUNCTION__);
    ret_status = EX_ILLEGAL_ARGUMENT;
  } else {
    hdl_imported = true;
    std::lock_guard<decltype(cwb_callbacks_lock_)> lock_guard(cwb_callbacks_lock_);
    if (cwb_callbacks_.find(hdl) != cwb_callbacks_.end()) {
      ALOGE("%s: buffer(%p) already being handled", __FUNCTION__, hdl);
      ret_status = EX_ILLEGAL_ARGUMENT;
    }
  }

  if (ret_status == EX_NONE) {
    sdm::CwbConfig cwb_config = {};
    // Load CWB ROI configuration
    auto &roi = cwb_config.cwb_roi;
    roi.left = FLOAT(captureConfigs.rectROI.left);
    roi.top = FLOAT(captureConfigs.rectROI.top);
    roi.right = FLOAT(captureConfigs.rectROI.right);
    roi.bottom = FLOAT(captureConfigs.rectROI.bottom);

    // Load Downscaled Output Rectangle
    auto &ds_rect = cwb_config.cwb_downscaled_rect;
    ds_rect.left = FLOAT(captureConfigs.downscaleRect.left);
    ds_rect.top = FLOAT(captureConfigs.downscaleRect.top);
    ds_rect.right = FLOAT(captureConfigs.downscaleRect.right);
    ds_rect.bottom = FLOAT(captureConfigs.downscaleRect.bottom);

#define CFLAG_DUALBIT_TAP_POINT 2
#define CFLAG_PU_AS_CWB_ROI_OFFSET 4
#define CFLAG_AVOID_REFRESH_OFFSET 5
    auto &cflag = captureConfigs.captureControlFlag;
    // Load CWB control operations
    cwb_config.tap_point = static_cast<sdm::CwbTapPoint>(cflag & LSB_MASK(CFLAG_DUALBIT_TAP_POINT));
    cwb_config.pu_as_cwb_roi = BIT_TO_BOOL(cflag, CFLAG_PU_AS_CWB_ROI_OFFSET);
    cwb_config.avoid_refresh = BIT_TO_BOOL(cflag, CFLAG_AVOID_REFRESH_OFFSET);
    // Load input control flag to CWB config control flags.
    cwb_config.cwb_control_params.value = cflag;
    // Reset internal control flags
    cwb_config.cwb_control_params.internal_control_flags = 0;

    ALOGI(
        "CWB config: Tap_Point: %d CWB_ROI: (%f %f %f %f) CWB_downscale_rect: (%f %f %f %f) "
        "for display-%d",
        cwb_config.tap_point, roi.left, roi.top, roi.right, roi.bottom, ds_rect.left, ds_rect.top,
        ds_rect.right, ds_rect.bottom, display_type);

    // Submit the CWB request with output buffer
    sdm::DisplayError ret = sideband_->PostBuffer(cwb_config, hdl, display_type);
    if (ret != sdm::kErrorNone) {
      ret_status = EX_TRANSACTION_FAILED;
    } else {
      std::lock_guard<decltype(cwb_callbacks_lock_)> lock_guard(cwb_callbacks_lock_);
      cwb_callbacks_.insert({hdl, {display_type, callback}});
    }
  }

  if (ret_status != EX_NONE && hdl_imported) {
    // Need to unregister and delete snap handle on CWB request rejection/failure
    handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
  }

  return (ret_status == EX_NONE) ? ScopedAStatus::ok()
                                 : ScopedAStatus(AStatus_fromExceptionCode(ret_status));
}

void AmbientDataCaptureAIDL::NotifyCWBStatus(int32_t status, void *hdl) {
  std::shared_ptr<IAmbientDataCaptureCallback> callback = nullptr;
  int32_t display_type = 0;

  if (!hdl) {
    ALOGE("%s: Null buffer handle is detected to notify!", __FUNCTION__);
    return;
  }

  {
    std::lock_guard<decltype(cwb_callbacks_lock_)> lock_guard(cwb_callbacks_lock_);
    auto it = cwb_callbacks_.find(hdl);
    if (it != cwb_callbacks_.end()) {
      std::tie(display_type, callback) = it->second;
      cwb_callbacks_.erase(it);
    }
  }

  if (!callback) {
    ALOGE("%s: buffer handle(%p) not found", __FUNCTION__, hdl);
  } else {
    NativeHandle buffer =
        sdm::AIDLNativeHandleFromSnapHandle(reinterpret_cast<SnapHandle *>(hdl), false);
    ALOGI("%s: Notify the client about buffer (%p) status %d for display-%d.", __FUNCTION__, hdl,
          status, display_type);

    callback->notifyOutputBufferDone(status, buffer);
  }

  handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
}

}  // namespace ambientdatacapture
}  // namespace qacs
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
