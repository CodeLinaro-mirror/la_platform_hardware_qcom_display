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

#pragma once

#include <aidlcommonsupport/NativeHandle.h>
#include <binder/Status.h>
#include <log/log.h>
#include <utils/locker.h>
#include <utils/constants.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <unordered_map>
#include <display_config.h>
#include <core/core_interface.h>
#include <core/ipc_interface.h>
#include <core/display_interface.h>

#include <debug_callback_intf.h>
#include <ISnapMapper.h>
#include <CWBMetadata.h>

#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <aidl/android/hardware/graphics/composer3/IComposer.h>
#include <aidl/vendor/qti/hardware/display/config/Attributes.h>
#include <aidl/vendor/qti/hardware/display/config/DisplayType.h>
#include <aidl/vendor/qti/hardware/display/config/Rect.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/BnAmbientDataCapture.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/BnAmbientDataCaptureCallback.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/ADCDisplayConfigs.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/ADCAlgoConfigs.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/ADCCallbackEvents.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/ADCCallbackAlgoEvents.h>
#include <aidl/vendor/qti/hardware/qacs/ambientdatacapture/ADCCallbackSystemEvents.h>

#include "AidlComposerHandleImporter.h"
#include "sdm_display_intf_settings.h"
#include "sdm_display_intf_lifecycle.h"
#include "sdm_display_intf_drawcycle_v2.h"
#include "sdm_display_intf_sideband.h"
#include "hwc_common.h"

#include "image_algo_interface.h"  // imagealgointegration public API

namespace aidl::vendor::qti::hardware::qacs::ambientdatacapture {
class AmbientDataCaptureAIDL;
}
namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace qacs {
namespace ambientdatacapture {

namespace composer3 = aidl::android::hardware::graphics::composer3;
#ifdef COMPOSER3_V3
using DisplayConfiguration = composer3::DisplayConfiguration;
#endif
using HwcDisplayCapability = composer3::DisplayCapability;
using HwcDisplayConnectionType = composer3::DisplayConnectionType;
using HwcClientTargetProperty = composer3::ClientTargetProperty;
using ::aidl::vendor::qti::hardware::display::composer3::ComposerHandleImporter;
using ::aidl::vendor::qti::hardware::display::config::Attributes;
using ::aidl::vendor::qti::hardware::display::config::DisplayPortType;
using ::aidl::vendor::qti::hardware::display::config::DisplayType;
using ::aidl::vendor::qti::hardware::display::config::Rect;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCAlgoConfigs;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCCallbackAlgoEvents;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCCallbackEvents;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCCallbackSystemEvents;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::ADCDisplayConfigs;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::IAmbientDataCapture;
using ::aidl::vendor::qti::hardware::qacs::ambientdatacapture::IAmbientDataCaptureCallback;

using ::android::binder::Status;
using ndk::ScopedAStatus;
using sdm::Display;
using sdm::SDMDisplayLifeCycleIntf;
using sdm::SDMDisplaySettingsIntf;
using sdm::SDMDisplaySideBandIntf;
using sdm::SDMSideBandCompositorCbIntf;

#ifdef COMPOSER3_V3
#define SDMDisplayDrawCycleIntfV SDMDisplayDrawCycleIntfV2
#else
#define SDMDisplayDrawCycleIntfV SDMDisplayDrawCycleIntf
#endif

using ::sdm::DebugCallbackIntf;
using sdm::SDMDisplayDrawCycleIntfV;
using ::vendor::qti::hardware::display::snapalloc::ISnapMapper;

typedef vendor_qti_hardware_display_common_cwb_metadata SnapCWBMetadata;

constexpr static int max_enqueue_count = 11;
constexpr static int timeout_ms = 30000;

// TODO : this struct will be translated to aidl
#define ALGO_CONFIG_SIZE 4096
struct ADCAlgoConfigsStructBlob {
  /*
   * @brief blob
   */
  size_t dataSize;
  char data[ALGO_CONFIG_SIZE];
};

class AmbientDataCaptureCallback : public BnAmbientDataCaptureCallback {
 public:
  AmbientDataCaptureCallback(const std::function<void()> &callback) : m_callback_(callback) {}

 private:
  std::function<void()> m_callback_;
};

class AmbientDataCaptureAIDL : public BnAmbientDataCapture, public SDMSideBandCompositorCbIntf {
 public:
  AmbientDataCaptureAIDL();
  ~AmbientDataCaptureAIDL();

  // AmbientDataCapture interface methods for display
  ScopedAStatus getActiveConfig(DisplayType dpy, int32_t *config) override;
  ScopedAStatus getDisplayAttributes(int32_t config_index, DisplayType dpy,
                                     Attributes *attributes) override;
  ScopedAStatus setAlgoConfig(const ADCAlgoConfigs &algoConfigs) override;
  ScopedAStatus captureOutputBuffer(const std::shared_ptr<IAmbientDataCaptureCallback> &callback,
                                    const ADCDisplayConfigs &captureConfigs) override;

  // Callback notification method from SDMSideBandCompositorCbIntf
  void NotifyCWBStatus(int32_t status, void *buffer) override;

  // Stub implementations for other SDMSideBandCompositorCbIntf methods
  void NotifyQsyncChange(uint64_t display_id, bool qsync_enabled, uint32_t refresh_rate,
                         uint32_t qsync_refresh_rate) {}
  void NotifyCameraSmoothInfo(sdm::SDMCameraSmoothOp op, int32_t fps) {}
  void NotifyResolutionChange(uint64_t display_id, sdm::SDMConfigAttributes &attr) {}
  void NotifyTUIEventDone(uint32_t ret, uint32_t disp_id, sdm::SDMTUIEventType type) {}
  void NotifyIdleStatus(bool status) {}
  void NotifyContentFps(const std::string &name, int32_t fps) {}
  void OnHdmiHotplug(bool connected) {}
  void OnCECMessageReceived(char *message, int len) {}
  void InitColorConvert(uint64_t display, bool secure) {}
  void ColorConvertBlit(uint64_t display, sdm::ColorConvertBlitContext *ctx) {}
  void ResetColorConvert(uint64_t display) {}
  void DestroyColorConvert(uint64_t display) {}
  void StartHistogram(uint64_t display, int max_frames) {}
  void StopHistogram(uint64_t display, bool teardown) {}
  void NotifyHistogram(uint64_t display, int fd, uint64_t blob_id, uint32_t panel_width,
                       uint32_t panel_height) {}
  std::string DumpHistogram(uint64_t display) { return ""; }
  void CollectHistogram(uint64_t display, uint64_t max_frames, uint64_t timestamp,
                        int32_t samples_size[NUM_HISTOGRAM_COLOR_COMPONENTS],
                        uint64_t *samples[NUM_HISTOGRAM_COLOR_COMPONENTS], uint64_t *numFrames) {}
  sdm::DisplayError GetHistogramAttributes(uint64_t display, int32_t *format, int32_t *dataspace,
                                           uint8_t *supported_components) {
    return sdm::kErrorNotSupported;
  }
  void StitchLayers(uint64_t display, sdm::LayerStitchContext *params) {}
  void InitLayerStitch(uint64_t display) {}
  void DestroyLayerStitch(uint64_t display) {}
  nsecs_t SystemTime(int clock) { return 0; }
  int GetDemuraFilePaths(const sdm::GenericPayload &in, sdm::GenericPayload *out) { return -1; }

 private:
  std::shared_ptr<SDMDisplaySettingsIntf> settings_;
  std::shared_ptr<SDMDisplayLifeCycleIntf> lifecycle_;
  std::shared_ptr<SDMDisplayDrawCycleIntfV> drawcycle_;
  std::shared_ptr<SDMDisplaySideBandIntf> sideband_;
  ComposerHandleImporter handle_importer_;

  std::mutex cwb_callbacks_lock_;
  std::unordered_map<void *, std::tuple<int32_t, std::shared_ptr<IAmbientDataCaptureCallback>>>
      cwb_callbacks_;

  void *snap_impl_lib_ = nullptr;
  std::shared_ptr<ISnapMapper> snapmapper_ = nullptr;

  // imagealgointegration SmartSelection adapter (optional).
  std::shared_ptr<imagealgo::SmartSelectionIntf> imagealgo_adapter_ = nullptr;
  std::string imagealgo_app_name_;
  std::atomic<uint32_t> ss_enqueue_count_{0};
  std::mutex imagealgo_lock_;

  int InitImageAlgoAdapter(const std::optional<std::vector<uint8_t>> &algoConfigsBlob);
  int DeInitImageAlgoAdapter();
  int FlushAllImageAlgoAdapter();
  int EnqueueImageAlgoAdapter(const std::optional<std::vector<uint8_t>> &algoConfigsBlob);
  int CreateEnqueuePayload(SnapHandle *handle, sdm::GenericPayload &enq_payload);
  // Emit callback invoked by SmartSelection pipeline when frames are selected/rejected.
  static void OnImageAlgoEmit(const imagealgo::SmartSelectionEmitResult *result, void *cookie);
  void ProcessImageAlgoResult(void *hdl, bool frame_selected);

  int GetSnapInstance();
  int AddCWBMetadata(SnapHandle *handle, int32_t display_type);
  void NotifyOutputBuffer(int32_t status, void *hdl);
};

}  // namespace ambientdatacapture
}  // namespace qacs
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
