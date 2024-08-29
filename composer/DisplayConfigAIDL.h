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
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfig.h>
#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfigCallback.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <binder/Status.h>
#include <log/log.h>
#include <utils/locker.h>
#include <config/device_interface.h>
#include <display_config.h>

#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <aidl/android/hardware/graphics/composer3/IComposer.h>
#include <binder/Status.h>
#include <hidl/HidlSupport.h>

#include <core/core_interface.h>
#include <core/ipc_interface.h>
#include <utils/locker.h>
#include <utils/constants.h>
#include <display_config.h>
#include <vector>
#include <queue>
#include <utility>
#include <future>  // NOLINT
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>
#include <core/display_interface.h>
#include "hwc_common.h"
#include "AidlComposerHandleImporter.h"

#include "sdm_display_intf_caps.h"
#include "sdm_display_intf_settings.h"
#include "sdm_display_intf_lifecycle.h"
#include "sdm_display_intf_drawcycle.h"
#include "sdm_display_intf_sideband.h"
#include "sdm_display_intf_layer_builder.h"
#include "QServiceBackend.h"

#include "histogram_collector.h"
#include "gl_color_convert.h"
#include "gl_layer_stitch.h"

namespace aidl::vendor::qti::hardware::display::config {
class DisplayConfigAIDL;
}

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace config {

typedef DisplayConfig::DisplayType DispType;

using ::android::sp;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
namespace composer3 = aidl::android::hardware::graphics::composer3;
#ifdef COMPOSER3_V3
using DisplayConfiguration = composer3::DisplayConfiguration;
#endif
using HwcDisplayCapability = composer3::DisplayCapability;
using HwcDisplayConnectionType = composer3::DisplayConnectionType;
using HwcClientTargetProperty = composer3::ClientTargetProperty;
using ::aidl::vendor::qti::hardware::display::composer3::ComposerHandleImporter;
using ::aidl::vendor::qti::hardware::display::config::Attributes;
using ::aidl::vendor::qti::hardware::display::config::CacV2Config;
using ::aidl::vendor::qti::hardware::display::config::CacV2ConfigExt;
using ::aidl::vendor::qti::hardware::display::config::CameraSmoothOp;
using ::aidl::vendor::qti::hardware::display::config::DisplayPortType;
using ::aidl::vendor::qti::hardware::display::config::IDisplayConfig;
using ::aidl::vendor::qti::hardware::display::config::IDisplayConfigCallback;
using ::aidl::vendor::qti::hardware::display::config::TUIEventType;

using ::android::binder::Status;
using ndk::ScopedAStatus;
using sdm::Display;
using sdm::GenericPayload;
using sdm::GLColorConvert;
using sdm::GLLayerStitch;
using sdm::GLRect;
using sdm::SDMDisplayCapsIntf;
using sdm::SDMDisplayDrawCycleIntf;
using sdm::SDMDisplayLayerBuilderIntf;
using sdm::SDMDisplayLifeCycleIntf;
using sdm::SDMDisplaySettingsIntf;
using sdm::SDMDisplaySideBandIntf;
using sdm::SDMSideBandCompositorCbIntf;
using sdm::HWC3::Error;

class DisplayConfigCallback : public BnDisplayConfigCallback {
 public:
  DisplayConfigCallback(const std::function<void()> &callback) : m_callback_(callback) {}

 private:
  std::function<void()> m_callback_;
};

class DisplayConfigAIDL : public BnDisplayConfig, public SDMSideBandCompositorCbIntf {
 public:
  DisplayConfigAIDL();
  int IsPowerModeOverrideSupported(uint32_t disp_id, bool *supported);
  int GetDispTypeFromPhysicalId(uint64_t physical_disp_id, DisplayType *disp_type);
  ScopedAStatus isDisplayConnected(DisplayType dpy, bool *connected) override;
  ScopedAStatus setDisplayStatus(DisplayType dpy, ExternalStatus status) override;
  ScopedAStatus configureDynRefreshRate(DynRefreshRateOp op, int refresh_rate) override;
  ScopedAStatus getConfigCount(DisplayType dpy, int *count) override;
  ScopedAStatus getActiveConfig(DisplayType dpy, int *config) override;
  ScopedAStatus setActiveConfig(DisplayType dpy, int config) override;
  ScopedAStatus getDisplayAttributes(int config_index, DisplayType dpy,
                                     Attributes *attributes) override;
  ScopedAStatus setPanelBrightness(int level) override;
  ScopedAStatus getPanelBrightness(int *level) override;
  ScopedAStatus minHdcpEncryptionLevelChanged(DisplayType dpy, int min_enc_level) override;
  ScopedAStatus refreshScreen() override;
  ScopedAStatus controlPartialUpdate(DisplayType dpy, bool enable) override;
  ScopedAStatus toggleScreenUpdate(bool on) override;
  ScopedAStatus setIdleTimeout(int value) override;
  ScopedAStatus getHDRCapabilities(DisplayType dpy, HDRCapsParams *aidl_return) override;
  ScopedAStatus setCameraLaunchStatus(int on) override;
  ScopedAStatus displayBWTransactionPending(bool *status) override;
  ScopedAStatus setDisplayAnimating(long display_id, bool animating) override;
  ScopedAStatus controlIdlePowerCollapse(bool enable, bool synchronous) override;
  ScopedAStatus getWriteBackCapabilities(bool *is_wb_ubwc_supported);
  ScopedAStatus setDisplayDppsAdROI(int display_id, int h_start, int h_end, int v_start, int v_end,
                                    int factor_in, int factor_out) override;
  ScopedAStatus updateVSyncSourceOnPowerModeOff() override;
  ScopedAStatus updateVSyncSourceOnPowerModeDoze() override;
  ScopedAStatus setPowerMode(int disp_id, PowerMode power_mode) override;
  ScopedAStatus isPowerModeOverrideSupported(int disp_id, bool *supported) override;
  ScopedAStatus isHDRSupported(int disp_id, bool *supported) override;
  ScopedAStatus isWCGSupported(int disp_id, bool *supported) override;
  ScopedAStatus setLayerAsMask(int disp_id, long layer_id) override;
  ScopedAStatus getDebugProperty(const std::string &prop_name, std::string *value) override;
  ScopedAStatus getActiveBuiltinDisplayAttributes(Attributes *attr) override;
  ScopedAStatus setPanelLuminanceAttributes(int disp_id, float min_lum, float max_lum) override;
  ScopedAStatus isBuiltInDisplay(int disp_id, bool *aidl_return) override;
  ScopedAStatus isAsyncVDSCreationSupported(bool *aidl_return) override;
  ScopedAStatus createVirtualDisplay(int width, int height, int format) override;
  ScopedAStatus getSupportedDSIBitClks(int disp_id, std::vector<long> *bit_clks) override;
  ScopedAStatus getDSIClk(int disp_id, long *bit_clk) override;
  ScopedAStatus setDSIClk(int disp_id, long bit_clk) override;
  ScopedAStatus setQsyncMode(int disp_id, QsyncMode mode) override;
  ScopedAStatus isSmartPanelConfig(int disp_id, int config_id, bool *is_smart) override;
  ScopedAStatus isRotatorSupportedFormat(int hal_format, bool ubwc, bool *supported) override;
  ScopedAStatus controlQsyncCallback(bool enable) override;
  ScopedAStatus sendTUIEvent(DisplayType dpy, TUIEventType event_type) override;
  ScopedAStatus getDisplayHwId(int disp_id, int *display_hw_id) override;
  ScopedAStatus setClientUp() override;
  ScopedAStatus getSupportedDisplayRefreshRates(DisplayType dpy,
                                                std::vector<int> *supported_refresh_rates) override;
  ScopedAStatus isRCSupported(int disp_id, bool *supported) override;
  ScopedAStatus controlIdleStatusCallback(bool enable) override;
  ScopedAStatus isSupportedConfigSwitch(int disp_id, int config, bool *supported) override;
  ScopedAStatus getDisplayType(long physical_disp_id, DisplayType *display_type) override;
  ScopedAStatus setCWBOutputBuffer(
      const std::shared_ptr<IDisplayConfigCallback> &in_callback, int32_t in_disp_id,
      const Rect &in_rect, bool in_post_processed,
      const ::aidl::android::hardware::common::NativeHandle &in_buffer) override;
  ScopedAStatus setCameraSmoothInfo(CameraSmoothOp in_op, int32_t in_fps);
  ScopedAStatus registerCallback(const std::shared_ptr<IDisplayConfigCallback> &in_callback,
                                 int64_t *aidl_return);
  ScopedAStatus unRegisterCallback(int64_t in_handle);
  ScopedAStatus notifyDisplayIdleState(const std::vector<int32_t> &display_ids) {
    return ScopedAStatus::ok();
  }
  ScopedAStatus getDisplayPortId(int32_t disp_id, int32_t *port_id) override;
  ScopedAStatus isCacV2Supported(int32_t disp_id, bool *_aidl_return) override;
  ScopedAStatus configureCacV2(int32_t disp_id, const CacV2Config &in_config,
                               bool in_enable) override;
  ScopedAStatus configureCacV2PerEye(int32_t disp_id, const CacV2Config &in_leftConfig,
                                     const CacV2Config &in_rightConfig, bool in_enable) override;
  ScopedAStatus configureCacV2ExtPerEye(int32_t disp_id, const CacV2ConfigExt &in_leftConfig,
                                        const CacV2ConfigExt &in_rightConfig,
                                        bool in_enable) override;
  ScopedAStatus allowIdleFallback() { return ScopedAStatus::ok(); }
#ifdef COMPOSER3_V3
  ScopedAStatus setContentFps(const std::string &in_name, int32_t in_fps) override;
#endif

  void NotifyQsyncChange(uint64_t display_id, bool qsync_enabled, uint32_t refresh_rate,
                         uint32_t qsync_refresh_rate) override;
  void NotifyCameraSmoothInfo(sdm::SDMCameraSmoothOp op, int32_t fps) override;
  void NotifyResolutionChange(uint64_t display_id, sdm::SDMConfigAttributes &attr) override;
  void NotifyTUIEventDone(uint32_t ret, uint32_t disp_id, sdm::SDMTUIEventType type) override;
  void NotifyIdleStatus(bool status) override;
  void NotifyCWBStatus(int32_t status, void *buffer) override;
  void NotifyContentFps(const std::string &name, int32_t fps) override;

  // gl color convert callbacks
  void InitColorConvert(uint64_t display, bool secure);
  void ColorConvertBlit(uint64_t display, sdm::ColorConvertBlitContext *ctx);
  void ResetColorConvert(uint64_t display);
  void DestroyColorConvert(uint64_t display);

  // histogram callbacks
  void StartHistogram(uint64_t display, int max_frames);
  void StopHistogram(uint64_t display, bool teardown);
  void NotifyHistogram(uint64_t display, int fd, uint64_t blob_id, uint32_t panel_width,
                       uint32_t panel_height);
  std::string DumpHistogram(uint64_t display);
  void CollectHistogram(uint64_t display, uint64_t max_frames, uint64_t timestamp,
                        int32_t samples_size[NUM_HISTOGRAM_COLOR_COMPONENTS],
                        uint64_t *samples[NUM_HISTOGRAM_COLOR_COMPONENTS], uint64_t *numFrames);
  sdm::DisplayError GetHistogramAttributes(uint64_t display, int32_t *format, int32_t *dataspace,
                                           uint8_t *supported_components);

  // other sdmclient callbacks
  void OnCECMessageReceived(char *message, int len);
  void OnHdmiHotplug(bool connected) override;
  int GetDemuraFilePaths(const GenericPayload &in, GenericPayload *out) override;

  void StitchLayers(uint64_t display, sdm::LayerStitchContext *ctx);
  void InitLayerStitch(uint64_t display);
  void DestroyLayerStitch(uint64_t display);

  sdm::nsecs_t SystemTime(int clock);

 private:
  std::weak_ptr<DisplayConfig::ConfigCallback> qsync_callback_;

  std::weak_ptr<DisplayConfig::ConfigCallback> callback_;
  std::unordered_map<int64_t, std::shared_ptr<IDisplayConfigCallback>> callback_clients_;
  std::mutex callbacks_lock_;
  uint64_t callback_client_id_ = 0;
  bool enable_aidl_idle_notification_ = false;

  std::shared_ptr<SDMDisplayCapsIntf> caps_;
  std::shared_ptr<SDMDisplaySettingsIntf> settings_;
  std::shared_ptr<SDMDisplayLifeCycleIntf> lifecycle_;
  std::shared_ptr<SDMDisplayDrawCycleIntf> drawcycle_;
  std::shared_ptr<SDMDisplaySideBandIntf> sideband_;
  std::shared_ptr<SDMDisplayLayerBuilderIntf> layer_builder_;
  sdm::Locker *locker_ = nullptr;
  ComposerHandleImporter handle_importer_;

  sdm::QServiceBackend *qservice_ = nullptr;

  std::mutex cwb_callbacks_lock_;
  std::unordered_map<void *, std::tuple<int32_t, std::shared_ptr<IDisplayConfigCallback>>>
      cwb_callbacks_;

  // sdmclient callbacks
  std::unordered_map<uint64_t, GLColorConvert *> color_convert_map_;
  std::unordered_map<uint64_t, histogram::HistogramCollector *> histogram_map_;
  std::unordered_map<uint64_t, GLLayerStitch *> layer_stitch_map_;
};

}  // namespace config
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
