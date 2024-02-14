/*
 * Copyright 2022 The Android Open Source Project
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
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <log/log.h>
#include <unordered_set>
#include <vector>
#include <string>
#include <aidl/vendor/qti/hardware/display/composer3/BnQtiComposer3Client.h>
#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfig.h>
#include <aidl/vendor/qti/hardware/display/config/BnDisplayConfigCallback.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <hidl/HidlSupport.h>
#include <IQClient.h>
#include <QService.h>
#include <SnapHandle.h>
#include <Error.h>

#include <core/buffer_allocator.h>

#include "AidlComposerHandleImporter.h"
#include "AidlComposerServiceWriter.h"
#include "hwc_common.h"
#include "hwc_buffer_allocator.h"
#include "hwc_socket_handler.h"

#include "sdm_display_intf_caps.h"
#include "sdm_display_intf_settings.h"
#include "sdm_display_intf_lifecycle.h"
#include "sdm_display_intf_drawcycle.h"
#include "sdm_display_intf_layer_builder.h"
#include "sdm_display_intf_sideband.h"

#include "sdm_interface_factory.h"
#include "sdm_compositor_cb_intf.h"
#include "QServiceBackend.h"

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
using aidl::android::hardware::graphics::composer3::DisplayConfiguration;
using aidl::android::hardware::graphics::composer3::DisplayConnectionType;
using aidl::android::hardware::graphics::composer3::DisplayContentSample;
using aidl::android::hardware::graphics::composer3::DisplayContentSamplingAttributes;
using aidl::android::hardware::graphics::composer3::DisplayIdentification;
using aidl::android::hardware::graphics::composer3::FormatColorComponent;
using aidl::android::hardware::graphics::composer3::HdrCapabilities;
using aidl::android::hardware::graphics::composer3::IComposerCallback;
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
using ::aidl::vendor::qti::hardware::display::config::IDisplayConfigCallback;
using HwcDisplayConnectionType = composer3::DisplayConnectionType;
using ::android::hardware::hidl_handle;
using ndk::ScopedAStatus;
using ndk::SpAIBinder;

using sdm::Fence;
using sdm::HWC3::Error;
using std::shared_ptr;

using sdm::HWCBufferAllocator;
using sdm::HWCSocketHandler;

using sdm::SDMCompositorCbIntf;
using sdm::SDMDisplayCapsIntf;
using sdm::SDMDisplayDrawCycleIntf;
using sdm::SDMDisplayLifeCycleIntf;
using sdm::SDMDisplaySettingsIntf;
using sdm::SDMDisplaySideBandIntf;
using sdm::SDMInterfaceFactory;
using sdm::SDMVsyncPeriodChangeTimeline;
using sdm::QServiceBackend;

using sdm::DisplayConfigVariableInfo;
using sdm::SDMDisplayLayerBuilderIntf;

using ::vendor::qti::hardware::display::snapalloc::SnapHandle;
using SnapError = ::vendor::qti::hardware::display::snapalloc::Error;
using ::vendor::qti::hardware::display::snapalloc::snap_handle_create;
using ::vendor::qti::hardware::display::snapalloc::snap_handle_delete;

class BufferCacheEntry {
 public:
  BufferCacheEntry();
  BufferCacheEntry(BufferCacheEntry &&other);

  BufferCacheEntry(const BufferCacheEntry &other) = delete;
  BufferCacheEntry &operator=(const BufferCacheEntry &other) = delete;

  BufferCacheEntry &operator=(SnapHandle *handle);
  ~BufferCacheEntry();

  SnapHandle *getHandle() const { return mHandle; }

 private:
  void clear();

  SnapHandle *mHandle = nullptr;
};

class AidlComposerClient : public BnComposerClient,
                           public qClient::BnQClient,
                           public SDMCompositorCbIntf {
 public:
  AidlComposerClient(){};
  virtual ~AidlComposerClient();
  bool init(SDMDisplayCapsIntf *caps,
            SDMDisplaySettingsIntf *settings,
            SDMDisplayLifeCycleIntf *lifecycle,
            SDMDisplayDrawCycleIntf *drawcycle,
            SDMDisplayLayerBuilderIntf *layers,
            SDMDisplaySideBandIntf *sideband);

  // SDM compositor callback overrides
  void OnHotplug(uint64_t in_display, bool in_connected) override;
  void OnRefresh(uint64_t in_display) override;
  void OnVsync(uint64_t in_display, int64_t in_timestamp, int32_t in_vsync_period_nanos) override;
  void OnSeamlessPossible(uint64_t in_display) override;
  void OnVsyncIdle(uint64_t in_display) override;
  void OnVsyncPeriodTimingChanged(uint64_t in_display,
                                  const SDMVsyncPeriodChangeTimeline &in_updated_timeline);

  void setOnClientDestroyed(std::function<void()> onClientDestroyed) {
    mOnClientDestroyed = onClientDestroyed;
  }

  ::android::status_t notifyCallback(uint32_t command, const ::android::Parcel *input_parcel,
                                     ::android::Parcel *output_parcel);

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

  ScopedAStatus getDisplayConfigurations(int64_t in_display, int32_t maxFrameIntervalNs,
                                         std::vector<DisplayConfiguration> *outConfigs) override;
  ScopedAStatus notifyExpectedPresent(int64_t displayId,
                                      const ClockMonotonicTimestamp &expectedPresentTime,
                                      int32_t frameIntervalNs) override;

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

  // Methods for ConcurrentWriteBack
  Error getDisplayReadbackBuffer(int64_t display, const SnapHandle *rawHandle);

  // Methods for extensions (QtiComposer3Client)
  ScopedAStatus executeQtiCommands(const std::vector<QtiDisplayCommand> &in_commands,
                                   std::vector<CommandResultPayload> *aidl_return);

 protected:
  SpAIBinder createBinder() override;

 private:
  std::unordered_map<int64_t, std::shared_ptr<IDisplayConfigCallback>> callback_clients_;
  HWCBufferAllocator buffer_allocator_{};

  struct LayerBuffers {
    std::vector<BufferCacheEntry> Buffers;
    // the handle is a sideband stream handle, not a buffer handle
    BufferCacheEntry SidebandStream;
  };

  struct DisplayData {
    bool IsVirtual;

    std::vector<BufferCacheEntry> ClientTargets;
    std::vector<BufferCacheEntry> OutputBuffers;

    std::unordered_map<sdm::LayerId, LayerBuffers> Layers;

    explicit DisplayData(bool isVirtual) : IsVirtual(isVirtual) {}
  };

  class CommandEngine {
   public:
    CommandEngine(AidlComposerClient &client) : mClient(client){};
    bool init();
    Error execute(const std::vector<DisplayCommand> &in_commands,
                  std::vector<CommandResultPayload> *aidl_return);
    Error qtiExecute(const std::vector<QtiDisplayCommand> &in_commands,
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
      ALOGW("%s: error: %s", function.c_str(), sdm::to_string(err).c_str());
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

    void executeSetExpectedPresentTimeInternal(
        int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime);
    void executeSetLayerBlockingRegion(int64_t display, int64_t layer,
                                       const std::vector<std::optional<Rect>> &blockingRegion);

    // Commands from extensions (QtiComposer3Client)
    void executeSetClientTarget_3_1(int64_t display, const ClientTarget &command);
    void executeSetDisplayElapseTime(int64_t display, uint64_t time);
    void executeSetLayerType(int64_t display, int64_t layer, sdm::LayerType type);
    void executeSetLayerFlag(int64_t display, int64_t layer, sdm::LayerFlag flag);

    Rect readRect();
    std::vector<Rect> readRegion(size_t count);
    FRect readFRect();
    AidlComposerClient &mClient;
    std::unique_ptr<ComposerServiceWriter> mWriter;
    int32_t mCommandIndex;

    // Buffer cache impl
    enum class BufferCache {
      CLIENT_TARGETS,
      OUTPUT_BUFFERS,
      LAYER_BUFFERS,
      LAYER_SIDEBAND_STREAMS,
    };

    Error lookupBufferCacheEntryLocked(int64_t display, int64_t layer, BufferCache cache,
                                       uint32_t slot, BufferCacheEntry **outEntry);
    Error lookupBuffer(int64_t display, int64_t layer, BufferCache cache, uint32_t slot,
                       bool useCache, SnapHandle **handle);
    Error updateBuffer(int64_t display, int64_t layer, BufferCache cache, uint32_t slot,
                       bool useCache, SnapHandle *handle);

    Error lookupLayerSidebandStream(int64_t display, int64_t layer, SnapHandle *handle) {
      return lookupBuffer(display, layer, BufferCache::LAYER_SIDEBAND_STREAMS, 0, false, &handle);
    }
    Error updateLayerSidebandStream(int64_t display, int64_t layer, SnapHandle *handle) {
      return updateBuffer(display, layer, BufferCache::LAYER_SIDEBAND_STREAMS, 0, false, handle);
    }
    Error postPresentDisplay(int64_t display, shared_ptr<Fence> *presentFence);
    Error postValidateDisplay(int64_t display, uint32_t &types_count, uint32_t &reqs_count);

    void GetSDMRectFromRect(const Rect *rect, sdm::SDMRegion *region) {
      for (int i = 0; i < region->num_rects; i++) {
        sdm::SDMRect new_rec = {rect[i].left, rect[i].top, rect[i].right, rect[i].bottom};
        region->rects.push_back(new_rec);
      }
    }
  };

  SDMDisplayCapsIntf *caps_ = nullptr;
  SDMDisplaySettingsIntf *settings_ = nullptr;
  SDMDisplayLifeCycleIntf *lifecycle_ = nullptr;
  SDMDisplayDrawCycleIntf *drawcycle_ = nullptr;
  SDMDisplayLayerBuilderIntf *layer_builder_ = nullptr;
  SDMDisplaySideBandIntf *sideband_ = nullptr;
  HWCSocketHandler socket_handler_;

  QServiceBackend *qservice_ = nullptr;

  std::shared_ptr<IComposerCallback> callback_ = nullptr;
  std::mutex m_command_mutex_;
  std::mutex m_display_data_mutex_;
  std::unique_ptr<CommandEngine> mCommandEngine;
  std::function<void()> mOnClientDestroyed;
  std::unordered_map<sdm::Display, DisplayData> mDisplayData;
};

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
