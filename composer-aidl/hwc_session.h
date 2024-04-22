/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __HWC_SESSION_H__
#define __HWC_SESSION_H__

#include <cutils/native_handle.h>
#include <config/device_interface.h>
#include <core/core_interface.h>
#include <utils/locker.h>
#include <qd_utils.h>
#include <display_config.h>
#include <vector>
#include <utility>
#include <map>
#include <string>

#include "hwc_callbacks.h"
#include "hwc_common.h"
#include "hwc_layers.h"
#include "hwc_display.h"
#include "hwc_display_builtin.h"
#include "hwc_display_pluggable.h"
#include "hwc_display_dummy.h"
#include "hwc_display_virtual.h"
#include "hwc_display_pluggable_test.h"
#include "hwc_color_manager.h"
#include "hwc_socket_handler.h"
#include "hwc_display_event_handler.h"
#include "hwc_buffer_sync_handler.h"

#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <aidl/android/hardware/graphics/composer3/IComposer.h>
#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfig.h>
#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfigCallback.h>

namespace composer_V3 = aidl::android::hardware::graphics::composer3;
using HwcDisplayCapability = composer_V3::DisplayCapability;
using HwcDisplayConnectionType = composer_V3::DisplayConnectionType;

namespace aidl::vendor::qti::hardware::display::config {
class DisplayConfigAIDL;
}

namespace sdm {
using ::android::hardware::Return;
using ::android::hardware::hidl_string;
using android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;


int32_t GetDataspaceFromColorMode(ColorMode mode);

typedef DisplayConfig::DisplayType DispType;

// Create a singleton uevent listener thread valid for life of hardware composer process.
// This thread blocks on uevents poll inside uevent library implementation. This poll exits
// only when there is a valid uevent, it can not be interrupted otherwise. Tieing life cycle
// of this thread with HWC session cause HWC deinitialization to wait infinitely for the
// thread to exit.
class HWCUEventListener {
 public:
  virtual ~HWCUEventListener() {}
  virtual void UEventHandler(const char *uevent_data, int length) = 0;
};

class HWCUEvent {
 public:
  HWCUEvent();
  static void UEventThread(HWCUEvent *hwc_event);
  void Register(HWCUEventListener *uevent_listener);
  inline bool InitDone() { return init_done_; }

 private:
  std::mutex mutex_;
  std::condition_variable caller_cv_;
  HWCUEventListener *uevent_listener_ = nullptr;
  bool init_done_ = false;
};

constexpr int32_t kDataspaceSaturationMatrixCount = 16;
constexpr int32_t kDataspaceSaturationPropertyElements = 9;
constexpr int32_t kPropertyMax = 256;

class HWCSession : HWCUEventListener, public qClient::BnQClient,
                   public HWCDisplayEventHandler, public DisplayConfig::ClientContext {
      friend class aidl::vendor::qti::hardware::display::config::DisplayConfigAIDL;
 public:
  enum HotPlugEvent {
    kHotPlugNone,
    kHotPlugEvent,
  };

  HWCSession();
  int Init();
  int Deinit();
  HWC3::Error CreateVirtualDisplayObj(uint32_t width, uint32_t height, int32_t *format,
                                      Display *out_display_id);

  template <typename... Args>
  HWC3::Error CallDisplayFunction(Display display, HWC3::Error (HWCDisplay::*member)(Args...),
                              Args... args) {
    if (display >= HWCCallbacks::kNumDisplays) {
      return HWC3::Error::BadDisplay;
    }

    SCOPE_LOCK(locker_[display]);
    auto status = HWC3::Error::BadDisplay;
    if (hwc_display_[display]) {
      auto hwc_display = hwc_display_[display];
      status = (hwc_display->*member)(std::forward<Args>(args)...);
    }
    return status;
  }

  template <typename... Args>
  HWC3::Error CallLayerFunction(Display display, LayerId layer,
                            HWC3::Error (HWCLayer::*member)(Args...), Args... args) {
    if (display >= HWCCallbacks::kNumDisplays) {
      return HWC3::Error::BadDisplay;
    }

    SCOPE_LOCK(locker_[display]);
    auto status = HWC3::Error::BadDisplay;
    if (hwc_display_[display]) {
      status = HWC3::Error::BadLayer;
      auto hwc_layer = hwc_display_[display]->GetHWCLayer(layer);
      if (hwc_layer != nullptr) {
        status = (hwc_layer->*member)(std::forward<Args>(args)...);
        if (hwc_display_[display]->GetGeometryChanges()) {
          hwc_display_[display]->ResetValidation();
        }
      }
    }
    return status;
  }

  // HWC2 Functions that require a concrete implementation in hwc session
  // and hence need to be member functions
  static HWCSession *GetInstance();
  void GetCapabilities(uint32_t *outCount, int32_t *outCapabilities);
  void Dump(uint32_t *out_size, char *out_buffer);

  HWC3::Error AcceptDisplayChanges(Display display);
  HWC3::Error CreateLayer(Display display, LayerId *out_layer_id);
  HWC3::Error CreateVirtualDisplay(uint32_t width, uint32_t height, int32_t *format,
                               Display *out_display_id);
  HWC3::Error DestroyLayer(Display display, LayerId layer);
  HWC3::Error DestroyVirtualDisplay(Display display);
  HWC3::Error PresentDisplay(Display display, int32_t *out_retire_fence);
  void RegisterCallback(CallbackCommand descriptor, void *callback_data, void *callback_fn);
  HWC3::Error SetOutputBuffer(Display display, buffer_handle_t buffer, int32_t releaseFence);
  HWC3::Error SetPowerMode(Display display, int32_t int_mode);
  HWC3::Error ValidateDisplay(Display display, uint32_t *out_num_types,
                          uint32_t *out_num_requests);
  HWC3::Error SetColorMode(Display display, int32_t /*ColorMode*/ int_mode);
  HWC3::Error SetColorModeWithRenderIntent(Display display, int32_t /*ColorMode*/ int_mode,
                                       int32_t /*RenderIntent*/ int_render_intent);
  HWC3::Error SetColorTransform(Display display, const std::vector<float> &matrix);
  HWC3::Error GetReadbackBufferAttributes(Display display,
                                      int32_t *format, int32_t *dataspace);
  HWC3::Error SetReadbackBuffer(Display display, const native_handle_t *buffer,
                            int32_t acquire_fence);
  HWC3::Error GetReadbackBufferFence(Display display, int32_t *release_fence);
  HWC3::Error getDisplayDecorationSupport(Display display, PixelFormat_V3 *format,
                                          AlphaInterpretation *alpha);
  uint32_t GetMaxVirtualDisplayCount();
  HWC3::Error GetDisplayIdentificationData(Display display, uint8_t *outPort,
                                       uint32_t *outDataSize, uint8_t *outData);
  HWC3::Error GetDisplayCapabilities(Display display,
                                     hidl_vec<HwcDisplayCapability> *capabilities);
  HWC3::Error GetDisplayBrightnessSupport(Display display, bool *outSupport);
  HWC3::Error SetDisplayBrightness(Display display, float brightness);

  // newly added
  HWC3::Error GetDisplayType(Display display, int32_t *out_type);
  HWC3::Error GetDisplayAttribute(Display display, Config config,
                              HwcAttribute attribute, int32_t *out_value);
  HWC3::Error GetActiveConfig(Display display, Config *out_config);
  HWC3::Error GetColorModes(Display display, uint32_t *out_num_modes,
                        int32_t /*ColorMode*/ *int_out_modes);
  HWC3::Error GetRenderIntents(Display display, int32_t /*ColorMode*/ int_mode,
                           uint32_t *out_num_intents, int32_t /*RenderIntent*/ *int_out_intents);
  HWC3::Error GetHdrCapabilities(Display display, uint32_t* out_num_types, int32_t* out_types,
                             float* out_max_luminance, float* out_max_average_luminance,
                             float* out_min_luminance);
  HWC3::Error GetPerFrameMetadataKeys(Display display, uint32_t *out_num_keys,
                                  int32_t *int_out_keys);
  HWC3::Error GetClientTargetSupport(Display display, uint32_t width, uint32_t height,
                                 int32_t format, int32_t dataspace);
  HWC3::Error GetDisplayName(Display display, uint32_t *out_size, char *out_name);
  HWC3::Error SetActiveConfig(Display display, Config config);
  HWC3::Error GetChangedCompositionTypes(Display display, uint32_t *out_num_elements,
                                     LayerId *out_layers, int32_t *out_types);
  HWC3::Error GetDisplayRequests(Display display, int32_t *out_display_requests,
                             uint32_t *out_num_elements, LayerId *out_layers,
                             int32_t *out_layer_requests);
  HWC3::Error GetReleaseFences(Display display, uint32_t *out_num_elements,
                           LayerId *out_layers, std::vector<int32_t> *out_fences);

  HWC3::Error SetClientTarget(Display display, buffer_handle_t target, int32_t acquire_fence,
                          int32_t dataspace, Region damage);
  HWC3::Error SetCursorPosition(Display display, LayerId layer, int32_t x, int32_t y);
  HWC3::Error GetDataspaceSaturationMatrix(int32_t /*Dataspace*/ int_dataspace, float *out_matrix);

  // Layer functions
  HWC3::Error SetLayerBuffer(Display display, LayerId layer, buffer_handle_t buffer,
                         int32_t acquire_fence);
  HWC3::Error SetLayerBlendMode(Display display, LayerId layer, int32_t int_mode);
  HWC3::Error SetLayerDisplayFrame(Display display, LayerId layer, Rect frame);
  HWC3::Error SetLayerPlaneAlpha(Display display, LayerId layer, float alpha);
  HWC3::Error SetLayerSourceCrop(Display display, LayerId layer, FRect crop);
  HWC3::Error SetLayerTransform(Display display, LayerId layer, Transform transform);
  HWC3::Error SetLayerZOrder(Display display, LayerId layer, uint32_t z);
  HWC3::Error SetLayerSurfaceDamage(Display display, LayerId layer, Region damage);
  HWC3::Error SetLayerVisibleRegion(Display display, LayerId layer, Region damage);
  HWC3::Error SetLayerCompositionType(Display display, LayerId layer, int32_t int_type);
  HWC3::Error SetLayerColor(Display display, LayerId layer, Color color);
  HWC3::Error SetLayerDataspace(Display display, LayerId layer, int32_t dataspace);
  HWC3::Error SetLayerPerFrameMetadata(Display display, LayerId layer,
                                   uint32_t num_elements, const int32_t *int_keys,
                                   const float *metadata);
  HWC3::Error SetLayerPerFrameMetadataBlobs(Display display, LayerId layer, uint32_t num_elements,
                                   const int32_t *int_keys, const uint32_t *sizes,
                                   const uint8_t *metadata);
  HWC3::Error SetLayerColorTransform(Display display, LayerId layer, const float *matrix);

  HWC3::Error GetDisplayConnectionType(Display display,
                                          HwcDisplayConnectionType *outType);
  HWC3::Error GetDisplayVsyncPeriod(Display display,
                                       VsyncPeriodNanos *out_vsync_period);
  HWC3::Error SetActiveConfigWithConstraints(Display display, Config config,
                    const VsyncPeriodChangeConstraints *vsync_period_change_constraints,
                    VsyncPeriodChangeTimeline *out_timeline);
  HWC3::Error GetOverlaySupport(OverlayProperties *supported_props);
  virtual int RegisterClientContext(std::shared_ptr<DisplayConfig::ConfigCallback> callback,
                                    DisplayConfig::ConfigInterface **intf);
  virtual void UnRegisterClientContext(DisplayConfig::ConfigInterface *intf);


  // HWCDisplayEventHandler
  virtual void DisplayPowerReset();

  HWC3::Error SetVsyncEnabled(Display display, bool int_enabled);
  HWC3::Error GetDozeSupport(Display display, int32_t *out_support);
  HWC3::Error GetDisplayConfigs(Display display, uint32_t *out_num_configs,
                            Config *out_configs);
  static HWC3::Error SetAutoLowLatencyMode(Display display, bool on);
  static HWC3::Error GetSupportedContentTypes(Display display,
                                          uint32_t *count, uint32_t *contentTypes);

  void Refresh(Display display);
  HWC3::Error GetVsyncPeriod(Display disp, uint32_t *vsync_period);

  static Locker locker_[HWCCallbacks::kNumDisplays];
  static Locker power_state_[HWCCallbacks::kNumDisplays];
  static Locker display_config_locker_;

 private:

  class DisplayConfigImpl: public DisplayConfig::ConfigInterface {
   public:
    explicit DisplayConfigImpl(std::weak_ptr<DisplayConfig::ConfigCallback> callback,
                               HWCSession *hwc_session);

   private:
    virtual int IsDisplayConnected(DispType dpy, bool *connected);
    virtual int SetDisplayStatus(DispType dpy, DisplayConfig::ExternalStatus status);
    virtual int ConfigureDynRefreshRate(DisplayConfig::DynRefreshRateOp op, uint32_t refresh_rate);
    virtual int GetConfigCount(DispType dpy, uint32_t *count);
    virtual int GetActiveConfig(DispType dpy, uint32_t *config);
    virtual int SetActiveConfig(DispType dpy, uint32_t config);
    virtual int GetDisplayAttributes(uint32_t config_index, DispType dpy,
                                     DisplayConfig::Attributes *attributes);
    virtual int SetPanelBrightness(uint32_t level);
    virtual int GetPanelBrightness(uint32_t *level);
    virtual int MinHdcpEncryptionLevelChanged(DispType dpy, uint32_t min_enc_level);
    virtual int RefreshScreen();
    virtual int ControlPartialUpdate(DispType dpy, bool enable);
    virtual int ToggleScreenUpdate(bool on);
    virtual int SetIdleTimeout(uint32_t value);
    virtual int GetHDRCapabilities(DispType dpy, DisplayConfig::HDRCapsParams *caps);
    virtual int SetCameraLaunchStatus(uint32_t on);
    virtual int DisplayBWTransactionPending(bool *status);
    virtual int SetDisplayAnimating(uint64_t display_id, bool animating);
    virtual int ControlIdlePowerCollapse(bool enable, bool synchronous);
    virtual int GetWriteBackCapabilities(bool *is_wb_ubwc_supported);
    virtual int SetDisplayDppsAdROI(uint32_t display_id, uint32_t h_start, uint32_t h_end,
                                    uint32_t v_start, uint32_t v_end, uint32_t factor_in,
                                    uint32_t factor_out);
    virtual int UpdateVSyncSourceOnPowerModeOff();
    virtual int UpdateVSyncSourceOnPowerModeDoze();
    virtual int SetPowerMode(uint32_t disp_id, DisplayConfig::PowerMode power_mode);
    virtual int IsPowerModeOverrideSupported(uint32_t disp_id, bool *supported);
    virtual int IsHDRSupported(uint32_t disp_id, bool *supported);
    virtual int IsWCGSupported(uint32_t disp_id, bool *supported);
    virtual int SetLayerAsMask(uint32_t disp_id, uint64_t layer_id);
    virtual int GetDebugProperty(const std::string prop_name, std::string value) {return -EINVAL;}
    virtual int GetDebugProperty(const std::string prop_name, std::string *value);
    virtual int GetActiveBuiltinDisplayAttributes(DisplayConfig::Attributes *attr);
    virtual int SetPanelLuminanceAttributes(uint32_t disp_id, float min_lum, float max_lum);
    virtual int IsBuiltInDisplay(uint32_t disp_id, bool *is_builtin);
    virtual int GetSupportedDSIBitClks(uint32_t disp_id,
                                       std::vector<uint64_t> bit_clks) {return -EINVAL;}
    virtual int GetSupportedDSIBitClks(uint32_t disp_id, std::vector<uint64_t> *bit_clks);
    virtual int GetDSIClk(uint32_t disp_id, uint64_t *bit_clk);
    virtual int SetDSIClk(uint32_t disp_id, uint64_t bit_clk);
    virtual int SetCWBOutputBuffer(uint32_t disp_id, const DisplayConfig::Rect rect,
                                   bool post_processed, const native_handle_t *buffer);
    virtual int SetQsyncMode(uint32_t disp_id, DisplayConfig::QsyncMode mode);

    std::weak_ptr<DisplayConfig::ConfigCallback> callback_;
    HWCSession *hwc_session_ = nullptr;
  };

  struct DisplayMapInfo {
    Display client_id = HWCCallbacks::kNumDisplays;        // mapped sf id for this display
    int32_t sdm_id = -1;                                         // sdm id for this display
    sdm:: DisplayType disp_type = kDisplayTypeMax;              // sdm display type
    bool test_pattern = false;                                 // display will show test pattern
    void Reset() {
      // Do not clear client id
      sdm_id = -1;
      disp_type = kDisplayTypeMax;
      test_pattern = false;
    }
  };

  static const int kExternalConnectionTimeoutMs = 500;
  static const int kCommitDoneTimeoutMs = 100;
  uint32_t throttling_refresh_rate_ = 60;
  void UpdateThrottlingRate();
  void SetNewThrottlingRate(uint32_t new_rate);

  void ResetPanel();
  int InitSupportedDisplaySlots();
  void InitSupportedNullDisplaySlots();
  int GetDisplayIndex(int dpy);
  int CreatePrimaryDisplay();
  void CreateDummyDisplay(Display client_id);
  int HandleBuiltInDisplays();
  int HandlePluggableDisplays(bool delay_hotplug);
  int HandleConnectedDisplays(HWDisplaysInfo *hw_displays_info, bool delay_hotplug);
  int HandleDisconnectedDisplays(HWDisplaysInfo *hw_displays_info);
  void DestroyDisplay(DisplayMapInfo *map_info);
  void DestroyPluggableDisplay(DisplayMapInfo *map_info);
  void DestroyNonPluggableDisplay(DisplayMapInfo *map_info);
  int GetVsyncPeriod(int disp);
  int GetConfigCount(int disp_id, uint32_t *count);
  int GetActiveConfigIndex(int disp_id, uint32_t *config);
  int SetActiveConfigIndex(int disp_id, uint32_t config);
  int ControlPartialUpdate(int dpy, bool enable);
  int DisplayBWTransactionPending(bool *status);
  int SetDisplayStatus(int disp_id, HWCDisplay::DisplayStatus status);
  int MinHdcpEncryptionLevelChanged(int disp_id, uint32_t min_enc_level);
  int IsWbUbwcSupported(bool *value);
  int SetIdleTimeout(uint32_t value);
  int ToggleScreenUpdate(bool on);
  int SetCameraLaunchStatus(uint32_t on);
  int SetDisplayDppsAdROI(uint32_t display_id, uint32_t h_start, uint32_t h_end,
                          uint32_t v_start, uint32_t v_end, uint32_t factor_in,
                          uint32_t factor_out);
  int ControlIdlePowerCollapse(bool enable, bool synchronous);
  int32_t SetDynamicDSIClock(int64_t disp_id, uint32_t bitrate);
  bool HasHDRSupport(HWCDisplay *hwc_display);
  int32_t getDisplayBrightness(uint32_t display, float *brightness);
  int32_t setDisplayBrightness(uint32_t display, float brightness);
  bool isSmartPanelConfig(uint32_t disp_id, uint32_t config_id);

  // Uevent handler
  virtual void UEventHandler(const char *uevent_data, int length);

  // service methods
  void StartServices();

  // QClient methods
  virtual android::status_t notifyCallback(uint32_t command, const android::Parcel *input_parcel,
                                           android::Parcel *output_parcel);
  void DynamicDebug(const android::Parcel *input_parcel);
  android::status_t SetFrameDumpConfig(const android::Parcel *input_parcel);
  android::status_t SetMaxMixerStages(const android::Parcel *input_parcel);
  android::status_t SetDisplayMode(const android::Parcel *input_parcel);
  android::status_t ConfigureRefreshRate(const android::Parcel *input_parcel);
  android::status_t QdcmCMDHandler(const android::Parcel *input_parcel,
                                   android::Parcel *output_parcel);
  android::status_t QdcmCMDDispatch(uint32_t display_id,
                                    const PPDisplayAPIPayload &req_payload,
                                    PPDisplayAPIPayload *resp_payload,
                                    PPPendingParams *pending_action);
  android::status_t GetDisplayAttributesForConfig(const android::Parcel *input_parcel,
                                                  android::Parcel *output_parcel);
  android::status_t GetVisibleDisplayRect(const android::Parcel *input_parcel,
                                          android::Parcel *output_parcel);
  android::status_t SetMixerResolution(const android::Parcel *input_parcel);
  android::status_t SetColorModeOverride(const android::Parcel *input_parcel);
  android::status_t SetColorModeWithRenderIntentOverride(const android::Parcel *input_parcel);

  android::status_t SetColorModeById(const android::Parcel *input_parcel);
  android::status_t SetColorModeFromClient(const android::Parcel *input_parcel);
  android::status_t getComposerStatus();
  android::status_t SetQSyncMode(const android::Parcel *input_parcel);
  android::status_t SetIdlePC(const android::Parcel *input_parcel);
  android::status_t RefreshScreen(const android::Parcel *input_parcel);
  android::status_t SetAd4RoiConfig(const android::Parcel *input_parcel);
  android::status_t SetDsiClk(const android::Parcel *input_parcel);
  android::status_t GetDsiClk(const android::Parcel *input_parcel, android::Parcel *output_parcel);
  android::status_t GetSupportedDsiClk(const android::Parcel *input_parcel,
                                       android::Parcel *output_parcel);
  android::status_t SetPanelLuminanceAttributes(const android::Parcel *input_parcel);

  void HotPlug(Display display, bool state);

  // Internal methods
  HWC3::Error ValidateDisplayInternal(Display display, uint32_t *out_num_types,
                                      uint32_t *out_num_requests);
  HWC3::Error PresentDisplayInternal(Display display, int32_t *out_retire_fence);
  void HandleSecureSession();
  void HandlePowerOnPending(Display display, int retire_fence);
  void HandleHotplugPending(Display disp_id, int retire_fence);
  bool IsPluggableDisplayConnected();
  Display GetActiveBuiltinDisplay();
  void HandlePendingRefresh();
  void NotifyClientStatus(bool connected);

  CoreInterface *core_intf_ = nullptr;
  HWCDisplay *hwc_display_[HWCCallbacks::kNumDisplays] = {nullptr};
  HWCCallbacks callbacks_;
  HWCBufferAllocator buffer_allocator_;
  HWCBufferSyncHandler buffer_sync_handler_;
  HWCColorManager *color_mgr_ = nullptr;
  DisplayMapInfo map_info_primary_;                 // Primary display (either builtin or pluggable)
  std::vector<DisplayMapInfo> map_info_builtin_;    // Builtin displays excluding primary
  std::vector<DisplayMapInfo> map_info_pluggable_;  // Pluggable displays excluding primary
  std::vector<DisplayMapInfo> map_info_virtual_;    // Virtual displays
  std::vector<bool> is_hdr_display_;    // info on HDR supported
  std::map <Display, Display> map_hwc_display_;  // Real and dummy display pairs.
  bool reset_panel_ = false;
  bool client_connected_ = false;
  bool new_bw_mode_ = false;
  bool need_invalidate_ = false;
  int bw_mode_release_fd_ = -1;
  qService::QService *qservice_ = nullptr;
  HWCSocketHandler socket_handler_;
  bool hdmi_is_primary_ = false;
  bool is_composer_up_ = false;
  Locker callbacks_lock_;
  std::mutex mutex_lum_;
  int hpd_bpp_ = 0;
  int hpd_pattern_ = 0;
  static bool power_on_pending_[HWCCallbacks::kNumDisplays];
  static int null_display_mode_;
  HotPlugEvent hotplug_pending_event_ = kHotPlugNone;
  Locker pluggable_handler_lock_;
  bool destroy_virtual_disp_pending_ = false;
  uint32_t idle_pc_ref_cnt_ = 0;
  int32_t disable_hotplug_bwcheck_ = 0;
  int32_t disable_mask_layer_hint_ = 0;
  float set_max_lum_ = -1.0;
  float set_min_lum_ = -1.0;
  std::bitset<HWCCallbacks::kNumDisplays> pending_refresh_;
  bool async_powermode_ = false;
  bool disable_virtual_display_ = false;
  bool power_state_transition_[HWCCallbacks::kNumDisplays] = {};  // +1 to account for primary.
  std::bitset<HWCCallbacks::kNumDisplays> display_ready_;
  bool disable_get_screen_decorator_support_ = false;
};
}  // namespace sdm

#endif  // __HWC_SESSION_H__
