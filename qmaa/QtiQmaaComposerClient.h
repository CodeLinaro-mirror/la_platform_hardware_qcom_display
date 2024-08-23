/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2017 The Android Open Source Project
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
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QTIQMAACOMPOSERCLIENT_H__
#define __QTIQMAACOMPOSERCLIENT_H__

#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include <aidlcommonsupport/NativeHandle.h>
#include <log/log.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <unistd.h>

#include <utils/fence.h>
#include <utils/constants.h>

#include "QtiQmaaComposerCommandBuffer.h"
#include "QtiQmaaComposerHandleImporter.h"

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

#define TO_BINDER_STATUS(x) \
  x == 0 ? ndk::ScopedAStatus::ok() : ndk::ScopedAStatus::fromServiceSpecificError(x)

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::common::AlphaInterpretation;
using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::DisplayDecorationSupport;
using aidl::android::hardware::graphics::common::FRect;
using aidl::android::hardware::graphics::common::Hdr;
using aidl::android::hardware::graphics::common::HdrConversionCapability;
using aidl::android::hardware::graphics::common::HdrConversionStrategy;
using aidl::android::hardware::graphics::common::PixelFormat;
using aidl::android::hardware::graphics::common::Point;
using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::common::Transform;
using aidl::android::hardware::graphics::composer3::BnComposerClient;
using aidl::android::hardware::graphics::composer3::Buffer;
using aidl::android::hardware::graphics::composer3::ClientTarget;
using aidl::android::hardware::graphics::composer3::ClockMonotonicTimestamp;
using FColor = aidl::android::hardware::graphics::composer3::Color;
using aidl::android::hardware::graphics::composer3::ColorMode;
using aidl::android::hardware::graphics::composer3::CommandResultPayload;
using aidl::android::hardware::graphics::composer3::ContentType;
using aidl::android::hardware::graphics::composer3::DimmingStage;
using aidl::android::hardware::graphics::composer3::DisplayAttribute;
using aidl::android::hardware::graphics::composer3::DisplayBrightness;
using aidl::android::hardware::graphics::composer3::DisplayCapability;
using aidl::android::hardware::graphics::composer3::DisplayCommand;
#ifdef COMPOSER3_V3
using aidl::android::hardware::graphics::composer3::DisplayConfiguration;
#endif
using aidl::android::hardware::graphics::composer3::DisplayConnectionType;
using aidl::android::hardware::graphics::composer3::DisplayContentSample;
using aidl::android::hardware::graphics::composer3::DisplayContentSamplingAttributes;
using aidl::android::hardware::graphics::composer3::DisplayIdentification;
using aidl::android::hardware::graphics::composer3::FormatColorComponent;
using aidl::android::hardware::graphics::composer3::HdrCapabilities;
using aidl::android::hardware::graphics::composer3::IComposerCallback;
using aidl::android::hardware::graphics::composer3::IComposerClient;
using aidl::android::hardware::graphics::composer3::LayerBrightness;
using aidl::android::hardware::graphics::composer3::OverlayProperties;
using aidl::android::hardware::graphics::composer3::ParcelableBlendMode;
using aidl::android::hardware::graphics::composer3::ParcelableComposition;
using aidl::android::hardware::graphics::composer3::ParcelableDataspace;
using aidl::android::hardware::graphics::composer3::ParcelableTransform;
using aidl::android::hardware::graphics::composer3::PerFrameMetadata;
using aidl::android::hardware::graphics::composer3::PerFrameMetadataBlob;
using aidl::android::hardware::graphics::composer3::PerFrameMetadataKey;
using aidl::android::hardware::graphics::composer3::PlaneAlpha;
using aidl::android::hardware::graphics::composer3::PowerMode;
using aidl::android::hardware::graphics::composer3::ReadbackBufferAttributes;
using aidl::android::hardware::graphics::composer3::RenderIntent;
using aidl::android::hardware::graphics::composer3::VirtualDisplay;
using aidl::android::hardware::graphics::composer3::VsyncPeriodChangeConstraints;
using aidl::android::hardware::graphics::composer3::VsyncPeriodChangeTimeline;
using aidl::android::hardware::graphics::composer3::ZOrder;

typedef uint64_t Display;
typedef uint32_t Config;
typedef int64_t Layer;

namespace HWC3 {
enum class Error : int32_t {
  None = 0,
  BadConfig = IComposerClient::EX_BAD_CONFIG,
  BadDisplay = IComposerClient::EX_BAD_DISPLAY,
  BadLayer = IComposerClient::EX_BAD_LAYER,
  BadParameter = IComposerClient::EX_BAD_PARAMETER,
  HasChanges,
  NoResources = IComposerClient::EX_NO_RESOURCES,
  NotValidated = IComposerClient::EX_NOT_VALIDATED,
  Unsupported = IComposerClient::EX_UNSUPPORTED,
  SeamlessNotAllowed = IComposerClient::EX_SEAMLESS_NOT_ALLOWED,
};
}  // namespace HWC3

inline std::string to_string(HWC3::Error error) {
  switch (error) {
    case HWC3::Error::None:
      return "None";
    case HWC3::Error::BadConfig:
      return "BadConfig";
    case HWC3::Error::BadDisplay:
      return "BadDisplay";
    case HWC3::Error::BadLayer:
      return "BadLayer";
    case HWC3::Error::BadParameter:
      return "BadParameter";
    case HWC3::Error::HasChanges:
      return "HasChanges";
    case HWC3::Error::NoResources:
      return "NoResources";
    case HWC3::Error::NotValidated:
      return "NotValidated";
    case HWC3::Error::Unsupported:
      return "Unsupported";
    case HWC3::Error::SeamlessNotAllowed:
      return "SeamlessNotAllowed";
    default:
      return "Unknown";
  }
}

using ::android::sp;
using HWC3::Error;
using ndk::ScopedAStatus;
using sdm::Fence;
using std::string;

#define HWC_DISPLAY_PRIMARY 0

class BufferCacheEntry {
 public:
  BufferCacheEntry();
  BufferCacheEntry(BufferCacheEntry &&other);

  BufferCacheEntry(const BufferCacheEntry &other) = delete;
  BufferCacheEntry &operator=(const BufferCacheEntry &other) = delete;

  BufferCacheEntry &operator=(buffer_handle_t handle);
  ~BufferCacheEntry();

  buffer_handle_t getHandle() const { return mHandle; }

 private:
  void clear();

  buffer_handle_t mHandle;
};

class QtiComposerClient : public BnComposerClient {
 public:
  QtiComposerClient();
  virtual ~QtiComposerClient();
  static QtiComposerClient *qti_composerclient_instance_;

  void setOnClientDestroyed(std::function<void()> onClientDestroyed) {
    mOnClientDestroyed = onClientDestroyed;
  }

  // Methods from aidl::android::hardware::graphics::composer3::IComposerClient
  ScopedAStatus createLayer(int64_t in_display, int32_t in_buffer_slot_count,
                            int64_t *aidl_return) override;
  ScopedAStatus createVirtualDisplay(int32_t in_width, int32_t in_height,
                                     PixelFormat in_format_hint,
                                     int32_t in_output_buffer_slot_count,
                                     VirtualDisplay *aidl_return) override;
  ScopedAStatus destroyLayer(int64_t in_display, int64_t in_layer) override;
  ScopedAStatus destroyVirtualDisplay(int64_t in_display) override;
  ScopedAStatus executeCommands(const std::vector<DisplayCommand> &in_commands,
                                std::vector<CommandResultPayload> *aidl_return) override;
  ScopedAStatus getActiveConfig(int64_t in_display, int32_t *aidl_return) override;
  ScopedAStatus getColorModes(int64_t in_display, std::vector<ColorMode> *aidl_return) override;
  ScopedAStatus getDataspaceSaturationMatrix(Dataspace in_dataspace,
                                             std::vector<float> *aidl_return) override;
  ScopedAStatus getDisplayAttribute(int64_t in_display, int32_t in_config,
                                    DisplayAttribute in_attribute, int32_t *aidl_return) override;

#ifdef COMPOSER3_V3
  ScopedAStatus getDisplayConfigurations(int64_t in_display, int32_t maxFrameIntervalNs,
                                         std::vector<DisplayConfiguration> *outConfigs) override;
  ScopedAStatus notifyExpectedPresent(int64_t displayId,
                                      const ClockMonotonicTimestamp &expectedPresentTime,
                                      int32_t frameIntervalNs) override;
#endif

  ScopedAStatus getDisplayCapabilities(int64_t in_display,
                                       std::vector<DisplayCapability> *aidl_return) override;
  ScopedAStatus getDisplayConfigs(int64_t in_display, std::vector<int32_t> *aidl_return) override;
  ScopedAStatus getDisplayConnectionType(int64_t in_display,
                                         DisplayConnectionType *aidl_return) override;
  ScopedAStatus getDisplayIdentificationData(int64_t in_display,
                                             DisplayIdentification *aidl_return) override;
  ScopedAStatus getDisplayName(int64_t in_display, std::string *aidl_return) override;
  ScopedAStatus getDisplayVsyncPeriod(int64_t in_display, int32_t *aidl_return) override;
  ScopedAStatus getDisplayedContentSample(int64_t in_display, int64_t in_max_frames,
                                          int64_t in_timestamp,
                                          DisplayContentSample *aidl_return) override;
  ScopedAStatus getDisplayedContentSamplingAttributes(
      int64_t in_display, DisplayContentSamplingAttributes *aidl_return) override;
  ScopedAStatus getDisplayPhysicalOrientation(int64_t in_display, Transform *aidl_return) override;
  ScopedAStatus getHdrCapabilities(int64_t in_display, HdrCapabilities *aidl_return) override;
  ScopedAStatus getMaxVirtualDisplayCount(int32_t *aidl_return) override;
  ScopedAStatus getOverlaySupport(OverlayProperties *aidl_return) override;
  ScopedAStatus getPerFrameMetadataKeys(int64_t in_display,
                                        std::vector<PerFrameMetadataKey> *aidl_return) override;
  ScopedAStatus getReadbackBufferAttributes(int64_t in_display,
                                            ReadbackBufferAttributes *aidl_return) override;
  ScopedAStatus getReadbackBufferFence(int64_t in_display,
                                       ::ndk::ScopedFileDescriptor *aidl_return) override;
  ScopedAStatus getRenderIntents(int64_t in_display, ColorMode in_mode,
                                 std::vector<RenderIntent> *aidl_return) override;
  ScopedAStatus getSupportedContentTypes(int64_t in_display,
                                         std::vector<ContentType> *aidl_return) override;
  ScopedAStatus getDisplayDecorationSupport(
      int64_t in_display, std::optional<DisplayDecorationSupport> *aidl_return) override;
  ScopedAStatus registerCallback(const std::shared_ptr<IComposerCallback> &in_callback) override;
  ScopedAStatus setActiveConfig(int64_t in_display, int32_t in_config) override;
  ScopedAStatus setActiveConfigWithConstraints(
      int64_t in_display, int32_t in_config,
      const VsyncPeriodChangeConstraints &in_vsync_period_change_constraints,
      VsyncPeriodChangeTimeline *aidl_return) override;
  ScopedAStatus setBootDisplayConfig(int64_t in_display, int32_t in_config) override;
  ScopedAStatus clearBootDisplayConfig(int64_t in_display) override;
  ScopedAStatus getPreferredBootDisplayConfig(int64_t in_display, int32_t *aidl_return) override;
  ScopedAStatus setAutoLowLatencyMode(int64_t in_display, bool in_on) override;
  ScopedAStatus setClientTargetSlotCount(int64_t in_display,
                                         int32_t in_client_target_slot_count) override;
  ScopedAStatus setColorMode(int64_t in_display, ColorMode in_mode,
                             RenderIntent in_intent) override;
  ScopedAStatus setContentType(int64_t in_display, ContentType in_type) override;
  ScopedAStatus setDisplayedContentSamplingEnabled(int64_t in_display, bool in_enable,
                                                   FormatColorComponent in_component_mask,
                                                   int64_t in_max_frames) override;
  ScopedAStatus setPowerMode(int64_t in_display, PowerMode in_mode) override;
  ScopedAStatus setReadbackBuffer(int64_t in_display, const NativeHandle &in_buffer,
                                  const ::ndk::ScopedFileDescriptor &in_release_fence) override;
  ScopedAStatus setVsyncEnabled(int64_t in_display, bool in_enabled) override;
  ScopedAStatus setIdleTimerEnabled(int64_t in_display, int32_t in_timeout_ms) override;
  ScopedAStatus getHdrConversionCapabilities(
      std::vector<HdrConversionCapability> *_aidl_return) override;
  ScopedAStatus setHdrConversionStrategy(const HdrConversionStrategy &in_conversionStrategy,
                                         Hdr *_aidl_return) override;
  ScopedAStatus setRefreshRateChangedCallbackDebugEnabled(int64_t in_display,
                                                          bool in_enabled) override;

  // Methods for callbacks
  void onHotplug(Display display, bool connected);
  void onRefresh(Display display);
  void onVsync(Display display, int64_t timestamp, int32_t vsync_period_nanos);
  void onVsyncPeriodTimingChanged(Display display,
                                  hwc_vsync_period_change_timeline_t *updatedTimeline);
  void onSeamlessPossible(Display display);
  void onVsyncIdle(Display display);

  // Methods for ConcurrentWriteBack
  Error getDisplayReadbackBuffer(Display display, const native_handle_t *rawHandle,
                                 const native_handle_t **outHandle);

  // Methods to check support of specific features like skip_validate
  std::unordered_set<DisplayCapability> mCapabilities;
  void getCapabilities();
  bool hasCapability(DisplayCapability capability) { return (mCapabilities.count(capability) > 0); }

 protected:
  ndk::SpAIBinder createBinder() override;

 private:
  struct LayerBuffers {
    std::vector<BufferCacheEntry> Buffers;
    // the handle is a sideband stream handle, not a buffer handle
    BufferCacheEntry SidebandStream;
  };

  struct QMAADisplayConfigVariableInfo {
    uint32_t x_pixels = 0;
    uint32_t y_pixels = 0;
    uint32_t fps = 0;
    uint32_t vsync_period_ns = 0;
    float x_dpi = 0.0f;
    float y_dpi = 0.0f;
    bool is_yuv = false;
    bool smart_panel = false;

    bool operator==(const QMAADisplayConfigVariableInfo &info) const {
      return ((x_pixels == info.x_pixels) && (y_pixels == info.y_pixels) && (x_dpi == info.x_dpi) &&
              (y_dpi == info.y_dpi) && (fps == info.fps) &&
              (vsync_period_ns == info.vsync_period_ns) && (is_yuv == info.is_yuv) &&
              (smart_panel == info.smart_panel));
    }
  };

  struct QMAADisplayConfigFixedInfo {
    bool underscan = false;
    bool secure = false;
    bool is_cmdmode = false;
    bool hdr_supported = false;
    bool hdr_plus = false;
    bool hdr_metadata_type_one = false;
    uint32_t hdr_eotf = 0;
    float max_luminance = 0.0f;
    float average_luminance = 0.0f;
    float min_luminance = 0.0f;
    bool partial_update = false;
  };

  const std::vector<uint8_t> edid_{
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x6D, 0x01, 0x00, 0x01, 0x00, 0x00,
      0x00, 0x1B, 0x10, 0x01, 0x03, 0x80, 0x50, 0x2D, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47,
      0x98, 0x27, 0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38,
      0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0x50, 0x1D, 0x74, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
      0xFE, 0x00, 0x4E, 0x75, 0x6C, 0x6C, 0x20, 0x44, 0x69, 0x73, 0x70, 0x6C, 0x61, 0x79, 0x0A,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD1};

  struct DisplayData {
    bool IsVirtual;

    std::vector<BufferCacheEntry> ClientTargets;
    std::vector<BufferCacheEntry> OutputBuffers;

    std::unordered_map<Layer, LayerBuffers> Layers;

    explicit DisplayData(bool isVirtual) : IsVirtual(isVirtual) {}
  };

  class CommandEngine {
   public:
    CommandEngine(QtiComposerClient &client) : mClient(client){};
    bool init();
    Error execute(const std::vector<DisplayCommand> &in_commands,
                  std::vector<CommandResultPayload> *aidl_return);
    Error validateDisplay(int64_t display);
    Error presentDisplay(int64_t display, shared_ptr<Fence> *presentFence);

    void reset() { mWriter->reset(); }

   private:
    template <typename field, typename... Args, typename... prototypeParams>
    void ExecuteCommand(field &commandField, void (CommandEngine::*func)(prototypeParams...),
                        Args &&...args) {
      if ((static_cast<bool>(commandField))) {
        (this->*func)(std::forward<Args>(args)...);
      }
    }
    __attribute__((always_inline)) inline void writeError(std::string function, Error err) {
      ALOGW("%s: error: %s", function.c_str(), to_string(err).c_str());
      mWriter->setError(mCommandIndex, INT32(err));
    }

    // Commands from aidl::android::hardware::graphics::composer3::IComposerClient follow.
    void executeSetColorTransform(int64_t display, const std::vector<float> &matrix);
    void executeSetClientTarget(int64_t display, const ClientTarget &command);
    void executeSetDisplayBrightness(uint64_t display, const DisplayBrightness &command);
    void executeSetOutputBuffer(uint64_t display, const Buffer &buffer);
    void executeValidateDisplay(int64_t display,
                                const std::optional<ClockMonotonicTimestamp> expectedPresentTime);
    void executePresentOrValidateDisplay(
        int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime);
    void executeAcceptDisplayChanges(int64_t display);
    void executePresentDisplay(int64_t display);

    void executeSetLayerCursorPosition(int64_t display, int64_t layer, const Point &cursorPosition);
    void executeSetLayerBuffer(int64_t display, int64_t layer, const Buffer &buffer);
    void executeSetLayerSurfaceDamage(int64_t display, int64_t layer,
                                      const std::vector<std::optional<Rect>> &damage);
    void executeSetLayerBlendMode(int64_t display, int64_t layer,
                                  const ParcelableBlendMode &blendMode);
    void executeSetLayerColor(int64_t display, int64_t layer, const FColor &color);
    void executeSetLayerComposition(int64_t display, int64_t layer,
                                    const ParcelableComposition &composition);
    void executeSetLayerDataspace(int64_t display, int64_t layer,
                                  const ParcelableDataspace &dataspace);
    void executeSetLayerDisplayFrame(int64_t display, int64_t layer, const Rect &rect);
    void executeSetLayerPlaneAlpha(int64_t display, int64_t layer, const PlaneAlpha &planeAlpha);
    void executeSetLayerSidebandStream(int64_t display, int64_t layer,
                                       const NativeHandle &sidebandStream);
    void executeSetLayerSourceCrop(int64_t display, int64_t layer, const FRect &sourceCrop);
    void executeSetLayerTransform(int64_t display, int64_t layer,
                                  const ParcelableTransform &transform);
    void executeSetLayerVisibleRegion(int64_t display, int64_t layer,
                                      const std::vector<std::optional<Rect>> &visibleRegion);
    void executeSetLayerZOrder(int64_t display, int64_t layer, const ZOrder &zOrder);
    void executeSetLayerPerFrameMetadata(
        int64_t display, int64_t layer,
        const std::vector<std::optional<PerFrameMetadata>> &perFrameMetadata);
    void executeSetLayerColorTransform(int64_t display, int64_t layer,
                                       const std::vector<float> &colorTransform);
    void executeSetLayerPerFrameMetadataBlobs(
        int64_t display, int64_t layer,
        const std::vector<std::optional<PerFrameMetadataBlob>> &perFrameMetadataBlob);
    void executeSetLayerBrightness(int64_t display, int64_t layer,
                                   const LayerBrightness &brightness);

    void executeSetLayerBlockingRegion(int64_t display, int64_t layer,
                                       const std::vector<std::optional<Rect>> &blockingRegion);

    Rect readRect();
    std::vector<Rect> readRegion(size_t count);
    FRect readFRect();
    QtiComposerClient &mClient;
    std::unique_ptr<ComposerServiceWriter> mWriter;
    int32_t mCommandIndex;
    Display mDisplay = 0;
    Layer mLayer = 0;

    // Buffer cache impl
    enum class BufferCache {
      CLIENT_TARGETS,
      OUTPUT_BUFFERS,
      LAYER_BUFFERS,
      LAYER_SIDEBAND_STREAMS,
    };

    Error lookupBufferCacheEntryLocked(BufferCache cache, uint32_t slot,
                                       BufferCacheEntry **outEntry);
    Error lookupBuffer(BufferCache cache, uint32_t slot, bool useCache, buffer_handle_t handle,
                       buffer_handle_t *outHandle);
    Error updateBuffer(BufferCache cache, uint32_t slot, bool useCache, buffer_handle_t handle);

    Error lookupLayerSidebandStream(buffer_handle_t handle, buffer_handle_t *outHandle) {
      return lookupBuffer(BufferCache::LAYER_SIDEBAND_STREAMS, 0, false, handle, outHandle);
    }
    Error updateLayerSidebandStream(buffer_handle_t handle) {
      return updateBuffer(BufferCache::LAYER_SIDEBAND_STREAMS, 0, false, handle);
    }
  };

  std::shared_ptr<IComposerCallback> callback_ = nullptr;
  int64_t layer_count_ = 0;
  std::mutex mCommandMutex;
  std::unique_ptr<CommandEngine> mCommandEngine;
  QMAADisplayConfigVariableInfo default_variable_config_ = {};
  QMAADisplayConfigFixedInfo default_fixed_config_ = {};
  std::function<void()> mOnClientDestroyed;

  std::mutex mDisplayDataMutex;
  std::unordered_map<Display, DisplayData> mDisplayData;
};

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl

#endif  // __QTIQMAACOMPOSERCLIENT_H__
