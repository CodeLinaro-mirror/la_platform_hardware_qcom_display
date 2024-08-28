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

#include <vector>
#include <string>

#include "QtiQmaaComposerClient.h"

#include "android/binder_auto_utils.h"
#include <android/binder_ibinder_platform.h>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

ComposerHandleImporter mHandleImporter;

BufferCacheEntry::BufferCacheEntry() : mHandle(nullptr) {}

BufferCacheEntry::BufferCacheEntry(BufferCacheEntry &&other) {
  mHandle = other.mHandle;
  other.mHandle = nullptr;
}

BufferCacheEntry &BufferCacheEntry::operator=(buffer_handle_t handle) {
  clear();
  mHandle = handle;
  return *this;
}

BufferCacheEntry::~BufferCacheEntry() {
  clear();
}

void BufferCacheEntry::clear() {
  if (mHandle) {
    mHandleImporter.freeBuffer(mHandle);
  }
}

QtiComposerClient::QtiComposerClient() {
  mHandleImporter.initialize();

  default_variable_config_.vsync_period_ns = 16600000;
  default_variable_config_.x_pixels = 1080;
  default_variable_config_.y_pixels = 1920;
  default_variable_config_.x_dpi = 300;
  default_variable_config_.y_dpi = 300;
  default_variable_config_.fps = 60;
  default_variable_config_.is_yuv = false;

  mCommandEngine = std::make_unique<CommandEngine>(*this);
  if (mCommandEngine == nullptr) {
    ALOGE("Unable to create command engine!");
  }

  if (!mCommandEngine->init()) {
    ALOGE("Unable to initialize command engine!");
  }
}

QtiComposerClient::~QtiComposerClient() {
  mDisplayData.clear();

  mHandleImporter.cleanup();

  if (mOnClientDestroyed) {
    mOnClientDestroyed();
  }
}

void QtiComposerClient::onHotplug(Display display, bool connected) {
  if (connected) {
    std::lock_guard<std::mutex> lock_d(mDisplayDataMutex);
    mDisplayData.emplace(display, DisplayData(false));
  }

  auto ret = callback_->onHotplug(display, connected);
  ALOGW_IF(!ret.isOk(), "failed to send onHotplug: %s. SF likely unavailable.",
           ret.getDescription().c_str());

  if (!connected) {
    // Trigger refresh to make sure disconnect event received/updated properly by SurfaceFlinger.
    // Wait for sufficient time to ensure sufficient resources are available to process connection.
    uint32_t vsync_period = 0;
    usleep(vsync_period * 2 / 1000);

    // Wait for the input command message queue to process before destroying the local display data.
    std::lock_guard<std::mutex> lock(mCommandMutex);
    std::lock_guard<std::mutex> lock_d(mDisplayDataMutex);
    mDisplayData.erase(display);
  }
}

void QtiComposerClient::onRefresh(Display display) {
  auto ret = callback_->onRefresh(display);
  ALOGW_IF(!ret.isOk(), "failed to send onRefresh: %s. SF likely unavailable.",
           ret.getDescription().c_str());
}

void QtiComposerClient::onVsync(Display display, int64_t timestamp, int32_t vsync_period_nanos) {
  auto ret = callback_->onVsync(display, timestamp, vsync_period_nanos);
  ALOGW_IF(!ret.isOk(), "failed to send onVsync: %s. SF likely unavailable.",
           ret.getDescription().c_str());
}

void QtiComposerClient::onVsyncPeriodTimingChanged(
    Display display, hwc_vsync_period_change_timeline_t *updatedTimeline) {
  VsyncPeriodChangeTimeline timeline = {updatedTimeline->newVsyncAppliedTimeNanos,
                                        static_cast<bool>(updatedTimeline->refreshRequired),
                                        updatedTimeline->refreshTimeNanos};

  auto ret = callback_->onVsyncPeriodTimingChanged(display, timeline);
  ALOGW_IF(!ret.isOk(), "failed to send onVsyncPeriodTimingChanged: %s. SF likely unavailable.",
           ret.getDescription().c_str());
}

void QtiComposerClient::onSeamlessPossible(Display display) {
  auto ret = callback_->onSeamlessPossible(display);
  ALOGW_IF(!ret.isOk(), "failed to send onSeamlessPossible: %s. SF likely unavailable.",
           ret.getDescription().c_str());
}

void QtiComposerClient::onVsyncIdle(Display display) {
  auto ret = callback_->onVsyncIdle(display);
  ALOGW_IF(!ret.isOk(), "failed to send onVsyncIdle: %s. SF likely unavailable.",
           ret.getDescription().c_str());
}

ScopedAStatus QtiComposerClient::createLayer(int64_t in_display, int32_t in_buffer_slot_count,
                                             int64_t *aidl_return) {
  *aidl_return = ++layer_count_;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::createVirtualDisplay(int32_t in_width, int32_t in_height,
                                                      PixelFormat in_format_hint,
                                                      int32_t in_output_buffer_slot_count,
                                                      VirtualDisplay *aidl_return) {
  uint64_t display = 1;

  aidl_return->display = display;
  aidl_return->format = in_format_hint;
  return TO_BINDER_STATUS(INT32(Error::None));
  ;
}

ScopedAStatus QtiComposerClient::destroyLayer(int64_t in_display, int64_t in_layer) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::destroyVirtualDisplay(int64_t in_display) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::executeCommands(const std::vector<DisplayCommand> &in_commands,
                                                 std::vector<CommandResultPayload> *aidl_return) {
  std::lock_guard<std::mutex> lock(mCommandMutex);

  Error error = mCommandEngine->execute(in_commands, aidl_return);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getActiveConfig(int64_t in_display, int32_t *aidl_return) {
  *aidl_return = 0;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getColorModes(int64_t in_display,
                                               std::vector<ColorMode> *aidl_return) {
  uint32_t count = 1;

  aidl_return->resize(count);
  auto out_modes = reinterpret_cast<ColorMode *>(
      reinterpret_cast<std::underlying_type<ColorMode>::type *>(aidl_return->data()));
  out_modes[0] = ColorMode::NATIVE;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDataspaceSaturationMatrix(Dataspace in_dataspace,
                                                              std::vector<float> *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayAttribute(int64_t in_display, int32_t in_config,
                                                     DisplayAttribute in_attribute,
                                                     int32_t *aidl_return) {
  HWC3::Error err = Error::None;

  switch (in_attribute) {
    case DisplayAttribute::VSYNC_PERIOD:
      *aidl_return = (int32_t)(default_variable_config_.vsync_period_ns);
      break;
    case DisplayAttribute::WIDTH:
      *aidl_return = (int32_t)(default_variable_config_.x_pixels);
      break;
    case DisplayAttribute::HEIGHT:
      *aidl_return = (int32_t)(default_variable_config_.y_pixels);
      break;
    case DisplayAttribute::DPI_X:
      *aidl_return = (int32_t)(default_variable_config_.x_dpi * 1000.0f);
      break;
    case DisplayAttribute::DPI_Y:
      *aidl_return = (int32_t)(default_variable_config_.y_dpi * 1000.0f);
      break;
    default:
      *aidl_return = -1;
      err = Error::Unsupported;
  }

  return TO_BINDER_STATUS(INT32(err));
}

ScopedAStatus QtiComposerClient::getDisplayConfigurations(
    int64_t in_display, int32_t maxFrameIntervalNs, std::vector<DisplayConfiguration> *outConfigs) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::notifyExpectedPresent(
    int64_t displayId, const ClockMonotonicTimestamp &expectedPresentTime,
    int32_t frameIntervalNs) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayCapabilities(
    int64_t in_display, std::vector<DisplayCapability> *aidl_return) {
  bool isBuiltin = true;
  if (isBuiltin) {
    // For now will not implement DozeSupport path

    aidl_return->push_back(DisplayCapability::SKIP_CLIENT_COLOR_TRANSFORM);
    aidl_return->push_back(DisplayCapability::BRIGHTNESS);
    aidl_return->push_back(DisplayCapability::PROTECTED_CONTENTS);
  }

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayConfigs(int64_t in_display,
                                                   std::vector<int32_t> *aidl_return) {
  uint32_t count = 1;
  aidl_return->resize(count);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayConnectionType(int64_t in_display,
                                                          DisplayConnectionType *aidl_return) {
  *aidl_return = DisplayConnectionType::INTERNAL;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayIdentificationData(int64_t in_display,
                                                              DisplayIdentification *aidl_return) {
  uint8_t port = 1;
  uint32_t size = (uint32_t)(edid_.size());

  aidl_return->port = port;
  aidl_return->data.resize(size);
  memcpy(aidl_return->data.data(), edid_.data(), size);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayName(int64_t in_display, std::string *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayVsyncPeriod(int64_t in_display, int32_t *aidl_return) {
  *aidl_return = (int32_t)(default_variable_config_.vsync_period_ns);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayedContentSample(int64_t in_display,
                                                           int64_t in_max_frames,
                                                           int64_t in_timestamp,
                                                           DisplayContentSample *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayedContentSamplingAttributes(
    int64_t in_display, DisplayContentSamplingAttributes *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayPhysicalOrientation(int64_t in_display,
                                                               Transform *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getHdrCapabilities(int64_t in_display,
                                                    HdrCapabilities *aidl_return) {
  aidl_return->maxLuminance = 0.0f;
  aidl_return->maxAverageLuminance = 0.0f;
  aidl_return->minLuminance = 0.0f;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getMaxVirtualDisplayCount(int32_t *aidl_return) {
  *aidl_return = -1;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getOverlaySupport(OverlayProperties *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getPerFrameMetadataKeys(
    int64_t in_display, std::vector<PerFrameMetadataKey> *aidl_return) {
  uint32_t count = (uint32_t)(PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL) + 1;
  aidl_return->resize(count);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getReadbackBufferAttributes(
    int64_t in_display, ReadbackBufferAttributes *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getReadbackBufferFence(int64_t in_display,
                                                        ::ndk::ScopedFileDescriptor *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getRenderIntents(int64_t in_display, ColorMode in_mode,
                                                  std::vector<RenderIntent> *aidl_return) {
  uint32_t count = 1;

  aidl_return->resize(count);
  auto out_intents = reinterpret_cast<RenderIntent *>(
      reinterpret_cast<std::underlying_type<RenderIntent>::type *>(aidl_return->data()));
  out_intents[0] = RenderIntent::COLORIMETRIC;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getSupportedContentTypes(int64_t in_display,
                                                          std::vector<ContentType> *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::getDisplayDecorationSupport(
    int64_t in_display, std::optional<DisplayDecorationSupport> *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::registerCallback(
    const std::shared_ptr<IComposerCallback> &in_callback) {
  callback_ = in_callback;

  onHotplug(0 /* Primary display */, true);
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setActiveConfig(int64_t in_display, int32_t in_config) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setActiveConfigWithConstraints(
    int64_t in_display, int32_t in_config,
    const VsyncPeriodChangeConstraints &in_vsync_period_change_constraints,
    VsyncPeriodChangeTimeline *aidl_return) {
  aidl_return->refreshRequired = false;
  aidl_return->refreshTimeNanos = 0;

  return TO_BINDER_STATUS(INT32(Error::SeamlessNotAllowed));
}
ScopedAStatus QtiComposerClient::setBootDisplayConfig(int64_t in_display, int32_t in_config) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::clearBootDisplayConfig(int64_t in_display) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::getPreferredBootDisplayConfig(int64_t in_display,
                                                               int32_t *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setAutoLowLatencyMode(int64_t in_display, bool in_on) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setClientTargetSlotCount(int64_t in_display,
                                                          int32_t in_client_target_slot_count) {
  std::lock_guard<std::mutex> lock(mDisplayDataMutex);

  auto dpy = mDisplayData.find(in_display);
  if (dpy == mDisplayData.end()) {
    return TO_BINDER_STATUS(INT32(Error::BadDisplay));
  }
  dpy->second.ClientTargets.resize(in_client_target_slot_count);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setColorMode(int64_t in_display, ColorMode in_mode,
                                              RenderIntent in_intent) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setContentType(int64_t in_display, ContentType in_type) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setDisplayedContentSamplingEnabled(
    int64_t in_display, bool in_enable, FormatColorComponent in_component_mask,
    int64_t in_max_frames) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setPowerMode(int64_t in_display, PowerMode in_mode) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setReadbackBuffer(
    int64_t in_display, const NativeHandle &in_buffer,
    const ::ndk::ScopedFileDescriptor &in_release_fence) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setVsyncEnabled(int64_t in_display, bool in_enabled) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiComposerClient::setIdleTimerEnabled(int64_t in_display, int32_t in_timeout_ms) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::getHdrConversionCapabilities(
    std::vector<HdrConversionCapability> *_aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setHdrConversionStrategy(
    const HdrConversionStrategy &in_conversionStrategy, Hdr *_aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiComposerClient::setRefreshRateChangedCallbackDebugEnabled(int64_t in_display,
                                                                           bool in_enabled) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

void QtiComposerClient::getCapabilities() {
  uint32_t count = 2;
  DisplayCapability *outCapabilities;
  std::vector<DisplayCapability> composer_caps(count);

  outCapabilities = composer_caps.data();
  outCapabilities[0] = DisplayCapability::SKIP_CLIENT_COLOR_TRANSFORM;
  outCapabilities[1] = DisplayCapability::BRIGHTNESS;
  composer_caps.resize(count);

  mCapabilities.reserve(count);
  for (auto cap : composer_caps) {
    mCapabilities.insert(cap);
  }
}

bool QtiComposerClient::CommandEngine::init() {
  mWriter = std::make_unique<ComposerServiceWriter>();

  return (mWriter != nullptr);
}

Error QtiComposerClient::CommandEngine::execute(const std::vector<DisplayCommand> &commands,
                                                std::vector<CommandResultPayload> *result) {
  // std::set<int64_t> displaysPendingBrightnessChange;
  mCommandIndex = 0;

  for (const auto &displayCmd : commands) {
    ExecuteCommand(displayCmd.brightness, &CommandEngine::executeSetDisplayBrightness,
                   displayCmd.display, *displayCmd.brightness);
    for (const auto &layerCmd : displayCmd.layers) {
      ExecuteCommand(layerCmd.cursorPosition, &CommandEngine::executeSetLayerCursorPosition,
                     displayCmd.display, layerCmd.layer, *layerCmd.cursorPosition);
      ExecuteCommand(layerCmd.buffer, &CommandEngine::executeSetLayerBuffer, displayCmd.display,
                     layerCmd.layer, *layerCmd.buffer);
      ExecuteCommand(layerCmd.damage, &CommandEngine::executeSetLayerSurfaceDamage,
                     displayCmd.display, layerCmd.layer, *layerCmd.damage);
      ExecuteCommand(layerCmd.blendMode, &CommandEngine::executeSetLayerBlendMode,
                     displayCmd.display, layerCmd.layer, *layerCmd.blendMode);
      ExecuteCommand(layerCmd.composition, &CommandEngine::executeSetLayerComposition,
                     displayCmd.display, layerCmd.layer, *layerCmd.composition);
      // AIDL definiton of LayerCommand Color which calls into executeSetLayerColor:
      // Sets the color of the given layer. If the composition type of the layer is not
      // Composition.SOLID_COLOR, this call must succeed and have no other effect.
      // Since the function depends on composition type to be set, executeSetLayerColor
      // has to be called after executeSetLayerComposition
      ExecuteCommand(layerCmd.color, &CommandEngine::executeSetLayerColor, displayCmd.display,
                     layerCmd.layer, *layerCmd.color);
      ExecuteCommand(layerCmd.dataspace, &CommandEngine::executeSetLayerDataspace,
                     displayCmd.display, layerCmd.layer, *layerCmd.dataspace);
      ExecuteCommand(layerCmd.displayFrame, &CommandEngine::executeSetLayerDisplayFrame,
                     displayCmd.display, layerCmd.layer, *layerCmd.displayFrame);
      ExecuteCommand(layerCmd.planeAlpha, &CommandEngine::executeSetLayerPlaneAlpha,
                     displayCmd.display, layerCmd.layer, *layerCmd.planeAlpha);
      ExecuteCommand(layerCmd.sidebandStream, &CommandEngine::executeSetLayerSidebandStream,
                     displayCmd.display, layerCmd.layer, *layerCmd.sidebandStream);
      ExecuteCommand(layerCmd.sourceCrop, &CommandEngine::executeSetLayerSourceCrop,
                     displayCmd.display, layerCmd.layer, *layerCmd.sourceCrop);
      ExecuteCommand(layerCmd.visibleRegion, &CommandEngine::executeSetLayerVisibleRegion,
                     displayCmd.display, layerCmd.layer, *layerCmd.visibleRegion);
      ExecuteCommand(layerCmd.transform, &CommandEngine::executeSetLayerTransform,
                     displayCmd.display, layerCmd.layer, *layerCmd.transform);
      ExecuteCommand(layerCmd.z, &CommandEngine::executeSetLayerZOrder, displayCmd.display,
                     layerCmd.layer, *layerCmd.z);
      ExecuteCommand(layerCmd.brightness, &CommandEngine::executeSetLayerBrightness,
                     displayCmd.display, layerCmd.layer, *layerCmd.brightness);
      ExecuteCommand(layerCmd.perFrameMetadata, &CommandEngine::executeSetLayerPerFrameMetadata,
                     displayCmd.display, layerCmd.layer, *layerCmd.perFrameMetadata);
      ExecuteCommand(layerCmd.perFrameMetadataBlob,
                     &CommandEngine::executeSetLayerPerFrameMetadataBlobs, displayCmd.display,
                     layerCmd.layer, *layerCmd.perFrameMetadataBlob);
      ExecuteCommand(layerCmd.blockingRegion, &CommandEngine::executeSetLayerBlockingRegion,
                     displayCmd.display, layerCmd.layer, *layerCmd.blockingRegion);
    }
    ExecuteCommand(displayCmd.colorTransformMatrix, &CommandEngine::executeSetColorTransform,
                   displayCmd.display, *displayCmd.colorTransformMatrix);
    ExecuteCommand(displayCmd.clientTarget, &CommandEngine::executeSetClientTarget,
                   displayCmd.display, *displayCmd.clientTarget);
    ExecuteCommand(displayCmd.virtualDisplayOutputBuffer, &CommandEngine::executeSetOutputBuffer,
                   displayCmd.display, *displayCmd.virtualDisplayOutputBuffer);
    ExecuteCommand(displayCmd.validateDisplay, &CommandEngine::executeValidateDisplay,
                   displayCmd.display, displayCmd.expectedPresentTime);
    ExecuteCommand(displayCmd.acceptDisplayChanges, &CommandEngine::executeAcceptDisplayChanges,
                   displayCmd.display);
    ExecuteCommand(displayCmd.presentDisplay, &CommandEngine::executePresentDisplay,
                   displayCmd.display);
    ExecuteCommand(displayCmd.presentOrValidateDisplay,
                   &CommandEngine::executePresentOrValidateDisplay, displayCmd.display,
                   displayCmd.expectedPresentTime);

    ++mCommandIndex;

    // TODO: Process brightness change on presentDisplay if both commands come in?????
    // if (displayCmd.validateDisplay || displayCmd.presentDisplay ||
    //     displayCmd.presentOrValidateDisplay) {
    //   displaysPendingBrightnessChange.erase(displayCmd.display);
    // } else if (DisplayCmd.brightness) {
    //   displaysPendingBrightnessChange.insert(displayCmd.display);
    // }
  }

  if (!mCommandIndex) {
    ALOGW("%s: No command found", __FUNCTION__);
  }

  *result = mWriter->getPendingCommandResults();
  reset();

  return (mCommandIndex) ? Error::None : Error::BadParameter;
}

void QtiComposerClient::CommandEngine::executeSetColorTransform(int64_t display,
                                                                const std::vector<float> &matrix) {}

void QtiComposerClient::CommandEngine::executeSetClientTarget(int64_t display,
                                                              const ClientTarget &command) {
  bool useCache = !command.buffer.handle;
  buffer_handle_t clientTarget =
      useCache ? nullptr : ::android::makeFromAidl(*command.buffer.handle);
  int slot = command.buffer.slot;

  auto err = lookupBuffer(BufferCache::CLIENT_TARGETS, slot, useCache, clientTarget, &clientTarget);
  if (err == Error::None) {
    err = updateBuffer(BufferCache::CLIENT_TARGETS, slot, useCache, clientTarget);
  }

  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiComposerClient::CommandEngine::executeSetDisplayBrightness(
    uint64_t display, const DisplayBrightness &command) {
  if (std::isnan(command.brightness) || command.brightness > 1.0f ||
      (command.brightness < 0.0f && command.brightness != -1.0f)) {
    writeError(__FUNCTION__, Error::BadParameter);
    return;
  }
}

void QtiComposerClient::CommandEngine::executeSetOutputBuffer(uint64_t display,
                                                              const Buffer &buffer) {
  bool useCache = !buffer.handle;
  auto slot = buffer.slot;
  buffer_handle_t outputBuffer = useCache ? nullptr : ::android::makeFromAidl(*buffer.handle);

  auto err = lookupBuffer(BufferCache::OUTPUT_BUFFERS, slot, useCache, outputBuffer, &outputBuffer);
  if (err == Error::None) {
    err = updateBuffer(BufferCache::OUTPUT_BUFFERS, slot, useCache, outputBuffer);
  }

  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}
void QtiComposerClient::CommandEngine::executeValidateDisplay(
    int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {
  std::vector<Layer> changedLayers;
  std::vector<Composition> compositionTypes;
  std::vector<Layer> requestedLayers;
  std::vector<int32_t> requestMasks;
  ClientTargetProperty clientTargetProperty;
  int display_reqs = 0;

  mWriter->setChangedCompositionTypes(display, static_cast<std::vector<int64_t>>(changedLayers),
                                      compositionTypes);
  mWriter->setDisplayRequests(display, display_reqs,
                              static_cast<std::vector<int64_t>>(requestedLayers), requestMasks);
  static constexpr float kBrightness = 1.f;
  DimmingStage dimmingStage = DimmingStage::NONE;
  mWriter->setClientTargetProperty(display, clientTargetProperty, kBrightness, dimmingStage);
}

void QtiComposerClient::CommandEngine::executePresentOrValidateDisplay(
    int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {
  // First try to Present as is.
  mClient.getCapabilities();
  if (true) {
    std::vector<Layer> layers;
    auto err = Error::None;
    if (err == Error::None) {
      mWriter->setPresentOrValidateResult(display, PresentOrValidate::Result::Presented);
    }
  }

  // Present has failed. We need to fallback to validate
  std::vector<Layer> changedLayers;
  std::vector<Composition> compositionTypes;
  std::vector<Layer> requestedLayers;
  std::vector<int32_t> requestMasks;
  int display_reqs = 0;

  auto err = Error::None;
  if (err == Error::None) {
    mWriter->setPresentOrValidateResult(display, PresentOrValidate::Result::Validated);
    mWriter->setChangedCompositionTypes(display, static_cast<std::vector<int64_t>>(changedLayers),
                                        compositionTypes);
    mWriter->setDisplayRequests(display, display_reqs,
                                static_cast<std::vector<int64_t>>(requestedLayers), requestMasks);
  } else {
    writeError(__FUNCTION__, err);
  }
}

void QtiComposerClient::CommandEngine::executeAcceptDisplayChanges(int64_t display) {}

void QtiComposerClient::CommandEngine::executePresentDisplay(int64_t display) {}

void QtiComposerClient::CommandEngine::executeSetLayerCursorPosition(int64_t display, int64_t layer,
                                                                     const Point &cursorPosition) {}

void QtiComposerClient::CommandEngine::executeSetLayerBuffer(int64_t display, int64_t layer,
                                                             const Buffer &buffer) {
  bool useCache = !buffer.handle;
  auto slot = buffer.slot;
  buffer_handle_t layerBuffer = useCache ? nullptr : ::android::makeFromAidl(*buffer.handle);

  auto error = lookupBuffer(BufferCache::LAYER_BUFFERS, slot, useCache, layerBuffer, &layerBuffer);
  if (error == Error::None) {
    error = updateBuffer(BufferCache::LAYER_BUFFERS, slot, useCache, layerBuffer);
  }

  if (error != Error::None) {
    writeError(__FUNCTION__, error);
  }
}

void QtiComposerClient::CommandEngine::executeSetLayerSurfaceDamage(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &damage) {}

void QtiComposerClient::CommandEngine::executeSetLayerBlendMode(
    int64_t display, int64_t layer, const ParcelableBlendMode &blendMode) {}

void QtiComposerClient::CommandEngine::executeSetLayerColor(int64_t display, int64_t layer,
                                                            const FColor &color) {}

void QtiComposerClient::CommandEngine::executeSetLayerComposition(
    int64_t display, int64_t layer, const ParcelableComposition &composition) {}

void QtiComposerClient::CommandEngine::executeSetLayerDataspace(
    int64_t display, int64_t layer, const ParcelableDataspace &dataspace) {}

void QtiComposerClient::CommandEngine::executeSetLayerDisplayFrame(int64_t display, int64_t layer,
                                                                   const Rect &rect) {}

void QtiComposerClient::CommandEngine::executeSetLayerPlaneAlpha(int64_t display, int64_t layer,
                                                                 const PlaneAlpha &planeAlpha) {}

void QtiComposerClient::CommandEngine::executeSetLayerSidebandStream(
    int64_t display, int64_t layer, const NativeHandle &sidebandStream) {
  writeError(__FUNCTION__, Error::Unsupported);
}

void QtiComposerClient::CommandEngine::executeSetLayerSourceCrop(int64_t display, int64_t layer,
                                                                 const FRect &sourceCrop) {}

void QtiComposerClient::CommandEngine::executeSetLayerTransform(
    int64_t display, int64_t layer, const ParcelableTransform &transform) {}

void QtiComposerClient::CommandEngine::executeSetLayerVisibleRegion(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &visibleRegion) {}

void QtiComposerClient::CommandEngine::executeSetLayerZOrder(int64_t display, int64_t layer,
                                                             const ZOrder &zOrder) {}

void QtiComposerClient::CommandEngine::executeSetLayerPerFrameMetadata(
    int64_t display, int64_t layer,
    const std::vector<std::optional<PerFrameMetadata>> &perFrameMetadata) {}

void QtiComposerClient::CommandEngine::executeSetLayerColorTransform(
    int64_t display, int64_t layer, const std::vector<float> &colorTransform) {}

void QtiComposerClient::CommandEngine::executeSetLayerPerFrameMetadataBlobs(
    int64_t display, int64_t layer,
    const std::vector<std::optional<PerFrameMetadataBlob>> &perFrameMetadataBlob) {}

void QtiComposerClient::CommandEngine::executeSetLayerBrightness(
    int64_t display, int64_t layer, const LayerBrightness &brightness) {}

void QtiComposerClient::CommandEngine::executeSetLayerBlockingRegion(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &blockingRegion) {
  writeError(__FUNCTION__, Error::Unsupported);
}

Error QtiComposerClient::CommandEngine::lookupBufferCacheEntryLocked(BufferCache cache,
                                                                     uint32_t slot,
                                                                     BufferCacheEntry **outEntry) {
  auto dpy = mClient.mDisplayData.find(mDisplay);
  if (dpy == mClient.mDisplayData.end()) {
    return Error::BadDisplay;
  }

  BufferCacheEntry *entry = nullptr;
  switch (cache) {
    case BufferCache::CLIENT_TARGETS:
      if (slot < dpy->second.ClientTargets.size()) {
        entry = &dpy->second.ClientTargets[slot];
      }
      break;
    case BufferCache::OUTPUT_BUFFERS:
      if (slot < dpy->second.OutputBuffers.size()) {
        entry = &dpy->second.OutputBuffers[slot];
      }
      break;
    case BufferCache::LAYER_BUFFERS: {
      auto ly = dpy->second.Layers.find(mLayer);
      if (ly == dpy->second.Layers.end()) {
        return Error::BadLayer;
      }
      if (slot < ly->second.Buffers.size()) {
        entry = &ly->second.Buffers[slot];
      }
    } break;
    case BufferCache::LAYER_SIDEBAND_STREAMS: {
      auto ly = dpy->second.Layers.find(mLayer);
      if (ly == dpy->second.Layers.end()) {
        return Error::BadLayer;
      }
      if (slot == 0) {
        entry = &ly->second.SidebandStream;
      }
    } break;
    default:
      break;
  }

  if (!entry) {
    ALOGW("invalid buffer slot");
    return Error::BadParameter;
  }

  *outEntry = entry;

  return Error::None;
}

Error QtiComposerClient::CommandEngine::lookupBuffer(BufferCache cache, uint32_t slot,
                                                     bool useCache, buffer_handle_t handle,
                                                     buffer_handle_t *outHandle) {
  if (useCache) {
    std::lock_guard<std::mutex> lock(mClient.mDisplayDataMutex);

    BufferCacheEntry *entry;
    Error error = lookupBufferCacheEntryLocked(cache, slot, &entry);
    if (error != Error::None) {
      return error;
    }

    // input handle is ignored
    *outHandle = entry->getHandle();
  } else if (cache == BufferCache::LAYER_SIDEBAND_STREAMS) {
    if (handle) {
      *outHandle = native_handle_clone(handle);
      if (*outHandle == nullptr) {
        return Error::NoResources;
      }
    }
  } else {
    if (!mHandleImporter.importBuffer(handle)) {
      return Error::NoResources;
    }

    *outHandle = handle;
  }

  return Error::None;
}

Error QtiComposerClient::CommandEngine::updateBuffer(BufferCache cache, uint32_t slot,
                                                     bool useCache, buffer_handle_t handle) {
  // handle was looked up from cache
  if (useCache) {
    return Error::None;
  }

  std::lock_guard<std::mutex> lock(mClient.mDisplayDataMutex);

  BufferCacheEntry *entry = nullptr;
  Error error = lookupBufferCacheEntryLocked(cache, slot, &entry);
  if (error != Error::None) {
    return error;
  }

  *entry = handle;
  return Error::None;
}

ndk::SpAIBinder QtiComposerClient::createBinder() {
  auto binder = BnComposerClient::createBinder();
  AIBinder_setInheritRt(binder.get(), true);
  return binder;
}

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
