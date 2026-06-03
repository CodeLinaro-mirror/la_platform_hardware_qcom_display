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
#include <dlfcn.h>

#include "AmbientDataCaptureAIDL.h"

#include "sdm_interface_factory.h"
#include "display_properties.h"

using ::aidl::android::hardware::common::NativeHandle;
using AlgoConfigType =
    aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCAlgoConfigs::AlgoConfigType;
using AlgoConfigCommandType =
    aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCAlgoConfigs::AlgoConfigCommandType;
using sdm::SDMInterfaceFactory;
using BufferUsage = vendor_qti_hardware_display_common_BufferUsage;

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
    lifecycle_->RegisterSideBandCallbackEx(this, true,
                                           sdm::SideBandCallbackClient::kAmbientDataCapture);
  }
  drawcycle_ =
      reinterpret_pointer_cast<SDMDisplayDrawCycleIntfV>(sdm_factory->CreateDrawCycleIntf());
  sideband_ = sdm_factory->CreateSideBandIntf();
}

AmbientDataCaptureAIDL::~AmbientDataCaptureAIDL() {
  if (lifecycle_) {
    lifecycle_->RegisterSideBandCallbackEx(this, false,
                                           sdm::SideBandCallbackClient::kAmbientDataCapture);
  }

  int ret = DeInitImageAlgoAdapter();
  ALOGI("%s: ImageAlgo adapter deinitialized ret %d", __FUNCTION__, ret);

  if (snapmapper_) {
    snapmapper_.reset();
  }
  if (snap_impl_lib_) {
    if (dlclose(snap_impl_lib_) != 0) {
      ALOGE("%s: Failed to close snap_impl_lib: %s", __FUNCTION__, dlerror());
    }
    snap_impl_lib_ = nullptr;
  }
}

int AmbientDataCaptureAIDL::InitImageAlgoAdapter(
    const std::optional<std::vector<uint8_t>> &algoConfigsBlob) {
  std::lock_guard<decltype(imagealgo_lock_)> lock_guard(imagealgo_lock_);
  if (imagealgo_adapter_) {
    ALOGI("%s: ImageAlgo SmartSelection adapter already initialized", __FUNCTION__);
    return EX_NONE;
  }

  if (!algoConfigsBlob.has_value()) {
    ALOGE("%s: algoConfigsBlob parameter is null", __FUNCTION__);
    return EX_ILLEGAL_ARGUMENT;
  }

  // Reinterpret the blob as ADCAlgoConfigsStructBlob struct
  const ADCAlgoConfigsStructBlob *configBlob =
      reinterpret_cast<const ADCAlgoConfigsStructBlob *>(algoConfigsBlob->data());
  if (configBlob && configBlob->dataSize > 0 && configBlob->dataSize <= ALGO_CONFIG_SIZE) {
    ALOGI("%s: ImageAlgo adapter config_json set", __FUNCTION__);
  } else {
    ALOGE("%s: Invalid blob data (dataSize=%zu)", __FUNCTION__,
          configBlob ? configBlob->dataSize : 0);
    return EX_ILLEGAL_ARGUMENT;
  }

  // Initialize the SmartSelection adapter once (shared across all displays).
  auto *factory = GetImageAlgoAdapterFactory();
  if (!factory) {
    ALOGW("%s: GetImageAlgoAdapterFactory() returned null — ImageAlgo disabled", __FUNCTION__);
    return EX_UNSUPPORTED_OPERATION;
  }

  imagealgo_adapter_ = factory->CreateAdapter<imagealgo::SmartSelectionIntf>();
  if (!imagealgo_adapter_) {
    ALOGW("%s: GetImageAlgoAdapterFactory() returned null — ImageAlgo disabled", __FUNCTION__);
    return EX_UNSUPPORTED_OPERATION;
  }

  int ret = imagealgo_adapter_->Init();
  if (ret != 0) {
    ALOGE("%s: ImageAlgo adapter Init() failed: %d", __FUNCTION__, ret);
    imagealgo_adapter_.reset();
    return ret;
  }

  // Initialize the SmartSelection pipeline with the emit callback.
  sdm::GenericPayload init_payload;
  imagealgo::SmartSelectionInitInput *init_input = nullptr;
  init_payload.CreatePayload(init_input);
  if (!init_input) {
    ALOGE("%s: ImageAlgo CreatePayload for Init failed, adapter disabled", __FUNCTION__);
    imagealgo_adapter_->Deinit();
    imagealgo_adapter_.reset();
    return EX_TRANSACTION_FAILED;
  }

  init_input->config_json = configBlob->data;
  init_input->callback = OnImageAlgoEmit;
  init_input->cookie = this;
  ret = imagealgo_adapter_->ProcessOps(imagealgo::kSSInit, init_payload, nullptr);
  if (ret != 0) {
    ALOGE("%s: ImageAlgo kSSInit failed: %d — adapter disabled", __FUNCTION__, ret);
    imagealgo_adapter_->Deinit();
    imagealgo_adapter_.reset();
    return ret;
  }

  ALOGI("%s: ImageAlgo SmartSelection adapter initialized", __FUNCTION__);
  return ret;
}

int AmbientDataCaptureAIDL::DeInitImageAlgoAdapter() {
  // SmartSelection pipeline teardown
  std::lock_guard<decltype(imagealgo_lock_)> lock_guard(imagealgo_lock_);
  if (!imagealgo_adapter_) {
    ALOGI("%s: ImageAlgo adapter is not initialized", __FUNCTION__);
    return EX_NONE;
  }

  sdm::GenericPayload wait_payload;
  imagealgo::SmartSelectionWaitInput *wait_input = nullptr;
  wait_payload.CreatePayload(wait_input);
  if (wait_input) {
    wait_input->timeout_ms = timeout_ms;
  }

  int ret = imagealgo_adapter_->ProcessOps(imagealgo::kSSWaitUntilIdle, wait_payload, nullptr);
  ALOGI("%s: ImageAlgo WaitUntilIdle (pre-flush) for ret=%d", __FUNCTION__, ret);

  sdm::GenericPayload flush_payload;
  ret = imagealgo_adapter_->ProcessOps(imagealgo::kSSFlushAll, flush_payload, nullptr);
  ALOGI("%s: ImageAlgo FlushAll for ret=%d", __FUNCTION__, ret);

  sdm::GenericPayload wait2_payload;
  imagealgo::SmartSelectionWaitInput *wait2_input = nullptr;
  wait2_payload.CreatePayload(wait2_input);
  if (wait2_input) {
    wait2_input->timeout_ms = timeout_ms;
  }
  ret = imagealgo_adapter_->ProcessOps(imagealgo::kSSWaitUntilIdle, wait2_payload, nullptr);
  ALOGI("%s: ImageAlgo WaitUntilIdle (post-flush) for ret=%d", __FUNCTION__, ret);

  imagealgo_adapter_->Deinit();
  imagealgo_adapter_.reset();
  ALOGI("%s: ImageAlgo adapter deinitialized", __FUNCTION__);
  return ret;
}

int AmbientDataCaptureAIDL::FlushSelectedImageAlgoAdapter() {
  std::shared_ptr<imagealgo::SmartSelectionIntf> local_adapter;
  std::string app_name;
  {
    std::lock_guard<decltype(imagealgo_lock_)> algo_lock(imagealgo_lock_);
    local_adapter = imagealgo_adapter_;
    app_name = imagealgo_app_name_;
  }
  if (!local_adapter) {
    ALOGI("%s: ImageAlgo adapter is not initialized", __FUNCTION__);
    return EX_NONE;
  }

  int selected_count = -1;
  sdm::GenericPayload query_payload;
  sdm::GenericPayload query_out_payload;
  imagealgo::SmartSelectionQuerySelectorInput *query_input = nullptr;
  imagealgo::SmartSelectionQuerySelectorOutput *query_output = nullptr;
  query_payload.CreatePayload(query_input);
  query_out_payload.CreatePayload(query_output);
  if (query_input) {
    query_input->app_name = app_name;
  }

  int ret = local_adapter->ProcessOps(imagealgo::kSSQuerySelectorStatus, query_payload,
                                      &query_out_payload);
  selected_count = (query_output && ret == 0) ? query_output->selected_count : -1;
  ALOGI("%s: ImageAlgo QuerySelectorStatus for app=%s: ret=%d selected_count=%d", __FUNCTION__,
        app_name.c_str(), ret, selected_count);

  if (selected_count <= 0) {
    return ret;
  }

  sdm::GenericPayload flush_sel_payload;
  imagealgo::SmartSelectionFlushSelectedInput *flush_sel_input = nullptr;
  flush_sel_payload.CreatePayload(flush_sel_input);
  if (flush_sel_input) {
    flush_sel_input->app_name = app_name;
  }

  ret = local_adapter->ProcessOps(imagealgo::kSSFlushSelected, flush_sel_payload, nullptr);
  ALOGI("%s: ImageAlgo FlushSelected for app=%s: ret=%d (flushed %d selected frames)", __FUNCTION__,
        app_name.c_str(), ret, selected_count);

  sdm::GenericPayload verify_payload;
  sdm::GenericPayload verify_out_payload;
  imagealgo::SmartSelectionQuerySelectorInput *verify_input = nullptr;
  imagealgo::SmartSelectionQuerySelectorOutput *verify_output = nullptr;
  verify_payload.CreatePayload(verify_input);
  verify_out_payload.CreatePayload(verify_output);
  if (verify_input) {
    verify_input->app_name = app_name;
  }

  int retq = local_adapter->ProcessOps(imagealgo::kSSQuerySelectorStatus, verify_payload,
                                       &verify_out_payload);
  int remaining = (verify_output && retq == 0) ? verify_output->selected_count : -1;
  ALOGI("%s: ImageAlgo QuerySelectorStatus after FlushSelected: ret=%d remaining=%d (expected 0)",
        __FUNCTION__, retq, remaining);
  if (remaining != 0) {
    ALOGW("%s: ImageAlgo QuerySelectorStatus after FlushSelected: expected 0 but got %d",
          __FUNCTION__, remaining);
  }
  return (ret != 0) ? ret : retq;
}

int AmbientDataCaptureAIDL::FlushAllImageAlgoAdapter() {
  std::shared_ptr<imagealgo::SmartSelectionIntf> local_adapter;
  {
    std::lock_guard<decltype(imagealgo_lock_)> algo_lock(imagealgo_lock_);
    local_adapter = imagealgo_adapter_;
  }
  if (!local_adapter) {
    ALOGI("%s: ImageAlgo adapter is not initialized", __FUNCTION__);
    return EX_NONE;
  }

  sdm::GenericPayload wait_payload;
  imagealgo::SmartSelectionWaitInput *wait_input = nullptr;
  wait_payload.CreatePayload(wait_input);
  if (wait_input) {
    wait_input->timeout_ms = timeout_ms;
  }

  int ret = local_adapter->ProcessOps(imagealgo::kSSWaitUntilIdle, wait_payload, nullptr);
  ALOGI("%s: ImageAlgo WaitUntilIdle (pre-flush) for ret=%d", __FUNCTION__, ret);

  sdm::GenericPayload flush_payload;
  ret = local_adapter->ProcessOps(imagealgo::kSSFlushAll, flush_payload, nullptr);
  ALOGI("%s: ImageAlgo FlushAll for ret=%d", __FUNCTION__, ret);
  return ret;
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
  int ret = EX_ILLEGAL_ARGUMENT;
  switch (algoConfigs.algoConfigType) {
    case AlgoConfigType::SMART_SEL: {
      switch (algoConfigs.algoConfigCommandType) {
        case AlgoConfigCommandType::INIT: {
          ret = InitImageAlgoAdapter(algoConfigs.algoConfigsBlob);
          break;
        }
        case AlgoConfigCommandType::ENQUEUE: {
          ret = EnqueueImageAlgoAdapter(algoConfigs.algoConfigsBlob);
          break;
        }
        case AlgoConfigCommandType::FLUSH_ALL: {
          ret = FlushAllImageAlgoAdapter();
          break;
        }
        case AlgoConfigCommandType::DEINIT: {
          ret = DeInitImageAlgoAdapter();
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default:
      break;
  }
  return ((ret == EX_NONE) ? ScopedAStatus::ok()
                           : ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT)));
}

int AmbientDataCaptureAIDL::EnqueueImageAlgoAdapter(
    const std::optional<std::vector<uint8_t>> &algoConfigsBlob) {
  std::lock_guard<decltype(imagealgo_lock_)> lock_guard(imagealgo_lock_);
  if (!imagealgo_adapter_) {
    ALOGW("%s: ImageAlgo adapter is not initialized", __FUNCTION__);
    return EX_NONE;
  }

  if (!algoConfigsBlob.has_value()) {
    ALOGE("%s: algoConfigsBlob parameter is null", __FUNCTION__);
    return EX_ILLEGAL_ARGUMENT;
  }

  // Reinterpret the blob as ADCAlgoConfigsStructBlob struct
  const ADCAlgoConfigsStructBlob *configBlob =
      reinterpret_cast<const ADCAlgoConfigsStructBlob *>(algoConfigsBlob->data());
  if (configBlob && configBlob->dataSize > 0 && configBlob->dataSize <= ALGO_CONFIG_SIZE) {
    // Copy the char data[] array into imagealgo_app_name_ as a string
    imagealgo_app_name_ =
        std::string(configBlob->data, strnlen(configBlob->data, configBlob->dataSize));
    ALOGI("%s: imagealgo_app_name_ set to '%s' (dataSize=%zu)", __FUNCTION__,
          imagealgo_app_name_.c_str(), configBlob->dataSize);
    return EX_NONE;
  }
  ALOGE("%s: Invalid blob data (dataSize=%zu)", __FUNCTION__,
        configBlob ? configBlob->dataSize : 0);
  return EX_ILLEGAL_ARGUMENT;
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
    sdm::DisplayError ret = sideband_->PostBufferWithOwner(cwb_config, hdl, display_type, this);
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

int AmbientDataCaptureAIDL::GetSnapInstance() {
  if (snapmapper_ != nullptr) {
    return EX_NONE;
  }

  const std::string snapalloc_lib_name = "vendor.qti.hardware.display.snapalloc-impl.so";
  snap_impl_lib_ = ::dlopen(snapalloc_lib_name.c_str(), RTLD_NOW);
  if (!snap_impl_lib_) {
    ALOGE("Dlopen error for snapalloc impl: %s", dlerror());
    return sdm::kErrorCriticalResource;
  }

  std::shared_ptr<ISnapMapper> (*LINK_FETCH_ISnapMapper)(DebugCallbackIntf *) = nullptr;
  *reinterpret_cast<void **>(&LINK_FETCH_ISnapMapper) =
      ::dlsym(snap_impl_lib_, "FETCH_ISnapMapper");
  if (LINK_FETCH_ISnapMapper) {
    snapmapper_ = LINK_FETCH_ISnapMapper(nullptr);
  }

  if (snapmapper_ == nullptr) {
    ALOGE("%s: Failed to link FETCH_ISnapMapper - %s", __FUNCTION__, strerror(errno));
    if (dlclose(snap_impl_lib_) != 0) {
      ALOGE("%s: Failed to close snap_impl_lib: %s", __FUNCTION__, dlerror());
    }
    snap_impl_lib_ = nullptr;
    return sdm::kErrorCriticalResource;
  }

  return EX_NONE;
}

int AmbientDataCaptureAIDL::AddCWBMetadata(SnapHandle *handle, int32_t display_type) {
  auto ret_status = EX_NONE;
  if (!handle) {
    ALOGE("%s: Null buffer handle is detected to notify!", __FUNCTION__);
    ret_status = EX_TRANSACTION_FAILED;
    return ret_status;
  }

  // Query the current display rotation to determine if buffer is rotated
  sdm::SDMTransform display_rotation = sdm::SDMTransform::TRANSFORM_NONE;

  SnapCWBMetadata set_cwbMetadata = {};
  set_cwbMetadata.capture_metadata.cwb_timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
  set_cwbMetadata.capture_metadata.cwb_rotation = static_cast<int32_t>(display_rotation);
  set_cwbMetadata.capture_metadata.cwb_sensitive_layer = false;

  set_cwbMetadata.algo_metadata.smart_selection_algo_metadata.smart_selection_score = 0;
  set_cwbMetadata.algo_metadata.smart_selection_algo_metadata.selected = false;
  std::string appIDString = imagealgo_app_name_;
  std::snprintf(set_cwbMetadata.algo_metadata.smart_selection_algo_metadata.app_id,
                sizeof(set_cwbMetadata.algo_metadata.smart_selection_algo_metadata.app_id), "%s",
                appIDString.c_str());

  auto status = snapmapper_->SetMetadata(*handle, CWB_METADATA, &set_cwbMetadata);
  if (status != ::vendor::qti::hardware::display::snapalloc::Error::NONE) {
    ALOGE("%s: setMetadata failed for CWB_METADATA %d", __FUNCTION__, status);
    ret_status = EX_TRANSACTION_FAILED;
  }
  return ret_status;
}

int AmbientDataCaptureAIDL::CreateEnqueuePayload(SnapHandle *handle,
                                                 sdm::GenericPayload &enq_payload) {
  auto ret_status = EX_NONE;
  if (!handle) {
    ALOGE("%s: Null buffer handle is detected!", __FUNCTION__);
    ret_status = EX_TRANSACTION_FAILED;
    return ret_status;
  }

  if (!snapmapper_) {
    ALOGE("%s: snapmapper_ is null!", __FUNCTION__);
    return EX_TRANSACTION_FAILED;
  }

  uint64_t gralloc_width = 0, gralloc_height = 0;
  uint32_t gralloc_stride = 0, gralloc_aligned_height = 0;
  int32_t gralloc_format = 0;
  snapmapper_->GetMetadata(*handle, WIDTH, &gralloc_width);
  snapmapper_->GetMetadata(*handle, HEIGHT, &gralloc_height);
  snapmapper_->GetMetadata(*handle, STRIDE, &gralloc_stride);
  snapmapper_->GetMetadata(*handle, ALIGNED_HEIGHT_IN_PIXELS, &gralloc_aligned_height);
  snapmapper_->GetMetadata(*handle, PIXEL_FORMAT_ALLOCATED, &gralloc_format);
  const char *fmt_str = (gralloc_format == sdm::kFormatRGB888) ? "RGB888" : "RGBA8888";

  imagealgo::SmartSelectionEnqueueInput *enq_input = nullptr;
  enq_payload.CreatePayload(enq_input);
  if (!enq_input) {
    ALOGE("%s: CreatePayload returned a Null", __FUNCTION__);
    ret_status = EX_TRANSACTION_FAILED;
    return ret_status;
  }

  const native_handle_t *qbuffer = reinterpret_cast<native_handle_t *>(handle);
  enq_input->buffer = const_cast<native_handle_t *>(qbuffer);
  enq_input->cookie = const_cast<native_handle_t *>(qbuffer);
  // Build metadata_json from BufferInfo (same fields as QaiorSS_FrameHandle_t.metadataJson).
  enq_input->metadata_json = std::string("{\"appName\":\"") + imagealgo_app_name_ +
                             "\","
                             "\"width\":" +
                             std::to_string(static_cast<uint32_t>(gralloc_width)) +
                             ","
                             "\"height\":" +
                             std::to_string(static_cast<uint32_t>(gralloc_height)) +
                             ","
                             "\"stride\":" +
                             std::to_string(static_cast<uint32_t>(gralloc_stride)) +
                             ","
                             "\"alignedHeight\":" +
                             std::to_string(static_cast<uint32_t>(gralloc_aligned_height)) +
                             ","
                             "\"format\":\"" +
                             fmt_str + "\"}";
  ALOGI("%s: ImageAlgo enqueue metadata_json: %s, cookie = %p", __FUNCTION__,
        enq_input->metadata_json.c_str(), enq_input->cookie);
  return ret_status;
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
    } else {
      ALOGE("%s: buffer handle(%p) not found", __FUNCTION__, hdl);
      return;
    }
  }

  if (!callback) {
    ALOGE("%s: buffer handle(%p) not found", __FUNCTION__, hdl);
    return;
  }

  if ((GetSnapInstance() != EX_NONE) || !snapmapper_ || (status != EX_NONE)) {
    ALOGE("%s: GetSnapInstance failed! or CWB capture failed with status %d", __FUNCTION__, status);
    NotifyOutputBuffer(status, hdl);
    return;
  }

  SnapHandle *handle = reinterpret_cast<SnapHandle *>(hdl);
  if (AddCWBMetadata(handle, display_type) == EX_NONE) {
    ALOGI("%s: AddCWBMetadata succeeded for buffer (%p) display-%d", __FUNCTION__, hdl,
          display_type);
  } else {
    ALOGE("%s: AddCWBMetadata failed for buffer (%p) display-%d", __FUNCTION__, hdl, display_type);
  }

  BufferUsage usage_flag;
  snapmapper_->GetMetadata(*handle, USAGE, &usage_flag);
  bool secure = (usage_flag & BufferUsage::PROTECTED);
  if (secure) {
    ALOGI("%s: Not enqueueing to ImageAlgo for secure buffer handle =%p", __FUNCTION__, handle);
    NotifyOutputBuffer(status, hdl);
    return;
  }

  std::shared_ptr<imagealgo::SmartSelectionIntf> local_adapter;
  {
    std::lock_guard<decltype(imagealgo_lock_)> algo_lock(imagealgo_lock_);
    local_adapter = imagealgo_adapter_;
  }

  if (!local_adapter) {
    ALOGI("%s: ImageAlgo enqueue failed for buffer handle =%p as it is not initialized",
          __FUNCTION__, handle);
    NotifyOutputBuffer(status, hdl);
    return;
  }

  int ret = 0;
  if (ss_enqueue_count_ >= max_enqueue_count) {
    ret = FlushSelectedImageAlgoAdapter();
    if (ret != 0) {
      ALOGI("%s: ImageAlgo flushSelected failed for enqueue count %d", __FUNCTION__,
            ss_enqueue_count_.load());
    }
  }

  // Enqueue the captured CWB buffer to the SmartSelection adapter.
  sdm::GenericPayload enq_payload;
  ret = CreateEnqueuePayload(handle, enq_payload);
  if (ret != 0) {
    ALOGE("%s: ImageAlgo CreateEnqueuePayload failed with ret=%d, notifying client", __FUNCTION__,
          ret);
    NotifyOutputBuffer(status, hdl);
    return;
  }

  ret = local_adapter->ProcessOps(imagealgo::kSSEnqueue, enq_payload, nullptr);
  ALOGI("%s: ImageAlgo enqueue for disp %d: ret=%d, buffer handle =%p", __FUNCTION__, display_type,
        ret, handle);

  // Handle enqueue failure
  if (ret != 0) {
    ALOGE("%s: ImageAlgo enqueue failed with ret=%d, notifying client", __FUNCTION__, ret);
    NotifyOutputBuffer(status, hdl);
    return;
  }
  ss_enqueue_count_++;
}

void AmbientDataCaptureAIDL::OnImageAlgoEmit(const imagealgo::SmartSelectionEmitResult *result,
                                             void *cookie) {
  if (!result || !cookie) {
    return;
  }

  AmbientDataCaptureAIDL *self = static_cast<AmbientDataCaptureAIDL *>(cookie);
  void *hdl = nullptr;
  ALOGI("%s: selected=%zu rejected=%zu", __FUNCTION__, result->selected_frames.size(),
        result->rejected_frames.size());

  for (const auto &f : result->selected_frames) {
    hdl = f.buffer;
    ALOGI("%s: SELECTED: type=%d buffer=%p fd=%d cookie=%p meta=%s", __FUNCTION__,
          static_cast<int>(f.type), f.buffer, f.parcel_fd, f.cookie, f.metadata_json.c_str());
    self->ProcessImageAlgoResult(hdl, true);
  }
  for (const auto &f : result->rejected_frames) {
    hdl = f.buffer;
    ALOGI("%s: REJECTED: type=%d buffer=%p fd=%d cookie=%p meta=%s", __FUNCTION__,
          static_cast<int>(f.type), f.buffer, f.parcel_fd, f.cookie, f.metadata_json.c_str());
    self->ProcessImageAlgoResult(hdl, false);
  }
}

void AmbientDataCaptureAIDL::ProcessImageAlgoResult(void *hdl, bool frame_selected) {
  if (!hdl) {
    ALOGE("%s: Null buffer handle is detected to notify!", __FUNCTION__);
    return;
  }
  std::shared_ptr<IAmbientDataCaptureCallback> callback = nullptr;
  int32_t display_type = 0;

  {
    std::lock_guard<decltype(cwb_callbacks_lock_)> lock_guard(cwb_callbacks_lock_);
    auto it = cwb_callbacks_.find(hdl);
    if (it != cwb_callbacks_.end()) {
      std::tie(display_type, callback) = it->second;
      cwb_callbacks_.erase(it);
      if (ss_enqueue_count_ > 0) {
        ss_enqueue_count_--;
      }
    }
  }

  if (!callback) {
    ALOGE("%s: buffer handle(%p) not found", __FUNCTION__, hdl);
    handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
    return;
  }

  SnapHandle *handle = reinterpret_cast<SnapHandle *>(hdl);
  if (snapmapper_) {
    SnapCWBMetadata set_cwbMetadata_SS = {};
    snapmapper_->GetMetadata(*handle, CWB_METADATA, &set_cwbMetadata_SS);
    set_cwbMetadata_SS.algo_metadata.smart_selection_algo_metadata.selected = frame_selected;
    auto ret = snapmapper_->SetMetadata(*handle, CWB_METADATA, &set_cwbMetadata_SS);
    if (ret != ::vendor::qti::hardware::display::snapalloc::Error::NONE) {
      ALOGE("%s: setMetadata of Smart Selection failed for CWB_METADATA %d", __FUNCTION__, ret);
    }
  } else {
    ALOGW("%s: snapmapper_ is null, skipping metadata update", __FUNCTION__);
  }

  NativeHandle buffer =
      sdm::AIDLNativeHandleFromSnapHandle(reinterpret_cast<SnapHandle *>(hdl), false);

  callback->notifyOutputBufferDone(EX_NONE, buffer);

  handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
}

void AmbientDataCaptureAIDL::NotifyOutputBuffer(int32_t status, void *hdl) {
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
    callback->notifyOutputBufferDone(status, buffer);
    handle_importer_.freeBuffer(static_cast<const SnapHandle *>(hdl));
  }
}

}  // namespace ambientdatacapture
}  // namespace qacs
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
