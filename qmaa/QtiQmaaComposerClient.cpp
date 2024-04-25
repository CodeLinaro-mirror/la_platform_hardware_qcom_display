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
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

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

bool QtiQmaaComposerClient::init() {
  mCommandEngine = std::make_unique<CommandEngine>(*this);
  if (mCommandEngine == nullptr) {
    return false;
  }

  if (!mCommandEngine->init()) {
    mCommandEngine = nullptr;
    return false;
  }

  return true;
}

QtiQmaaComposerClient::~QtiQmaaComposerClient() {}

void QtiQmaaComposerClient::EnableCallback(bool enable) {
  return;
}

ScopedAStatus QtiQmaaComposerClient::createLayer(int64_t in_display, int32_t in_buffer_slot_count,
                                                 int64_t *aidl_return) {
  LayerId layer = layer_count_++;
  *aidl_return = static_cast<int64_t>(layer);
  std::lock_guard<std::mutex> lock(m_display_data_mutex_);
  auto dpy = mDisplayData.find(in_display);
  // The display entry may have already been removed by onHotplug.
  if (dpy != mDisplayData.end()) {
    auto ly = dpy->second.Layers.emplace(layer, LayerBuffers()).first;
    ly->second.Buffers.resize(in_buffer_slot_count);
  } else {
    return TO_BINDER_STATUS(INT32(Error::BadDisplay));
  }

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::createVirtualDisplay(int32_t in_width, int32_t in_height,
                                                          PixelFormat in_format_hint,
                                                          int32_t in_output_buffer_slot_count,
                                                          VirtualDisplay *aidl_return) {
  int32_t format = static_cast<int32_t>(in_format_hint);
  uint64_t display = 1;

  std::lock_guard<std::mutex> lock(m_display_data_mutex_);
  auto dpy = mDisplayData.emplace(static_cast<Display>(display), DisplayData(true)).first;
  dpy->second.OutputBuffers.resize(in_output_buffer_slot_count);

  aidl_return->display = display;
  aidl_return->format = in_format_hint;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::destroyLayer(int64_t in_display, int64_t in_layer) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::destroyVirtualDisplay(int64_t in_display) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::executeCommands(
    const std::vector<DisplayCommand> &in_commands,
    std::vector<CommandResultPayload> *aidl_return) {
  std::lock_guard<std::mutex> lock(m_command_mutex_);
  Error error = mCommandEngine->execute(in_commands, aidl_return);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getActiveConfig(int64_t in_display, int32_t *aidl_return) {
  uint32_t config = 0;
  *aidl_return = config;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getColorModes(int64_t in_display,
                                                   std::vector<ColorMode> *aidl_return) {
  uint32_t count = 1;
  aidl_return->resize(count);
  auto out_modes = reinterpret_cast<ColorMode *>(
      reinterpret_cast<std::underlying_type<ColorMode>::type *>(aidl_return->data()));
  out_modes[0] = ColorMode::NATIVE;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDataspaceSaturationMatrix(Dataspace in_dataspace,
                                                                  std::vector<float> *aidl_return) {
  Error error = Error::None;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayAttribute(int64_t in_display, int32_t in_config,
                                                         DisplayAttribute in_attribute,
                                                         int32_t *aidl_return) {
  int32_t value = 0;
  Error error = Error::None;
  switch (in_attribute) {
    case DisplayAttribute::VSYNC_PERIOD:
      value = (int32_t)(default_variable_config_.vsync_period_ns);
      break;
    case DisplayAttribute::WIDTH:
      value = (int32_t)(default_variable_config_.x_pixels);
      break;
    case DisplayAttribute::HEIGHT:
      value = (int32_t)(default_variable_config_.y_pixels);
      break;
    case DisplayAttribute::DPI_X:
      value = (int32_t)(default_variable_config_.x_dpi * 1000.0f);
      break;
    case DisplayAttribute::DPI_Y:
      value = (int32_t)(default_variable_config_.y_dpi * 1000.0f);
      break;
    default:
      value = -1;
      error = Error::Unsupported;
  }
  *aidl_return = value;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayCapabilities(
    int64_t in_display, std::vector<DisplayCapability> *aidl_return) {
  return ScopedAStatus::ok();
}

ScopedAStatus QtiQmaaComposerClient::getDisplayConfigs(int64_t in_display,
                                                       std::vector<int32_t> *aidl_return) {
  uint32_t count = 1;
  aidl_return->resize(count);
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayConnectionType(int64_t in_display,
                                                              DisplayConnectionType *aidl_return) {
  *aidl_return = DisplayConnectionType::INTERNAL;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayIdentificationData(
    int64_t in_display, DisplayIdentification *aidl_return) {
  uint8_t port = 1;
  uint32_t size = 0;
  std::vector<uint8_t> data(size);

  size = (uint32_t)(edid_.size());
  data.resize(size);
  memcpy(data.data(), edid_.data(), size);
  aidl_return->data.resize(size);
  aidl_return->data = data;
  aidl_return->port = port;

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayName(int64_t in_display, std::string *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayVsyncPeriod(int64_t in_display,
                                                           int32_t *aidl_return) {
  VsyncPeriodNanos vsync_period;
  vsync_period = static_cast<VsyncPeriodNanos>((int32_t)(default_variable_config_.vsync_period_ns));
  *aidl_return = INT32(vsync_period);
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayedContentSample(int64_t in_display,
                                                               int64_t in_max_frames,
                                                               int64_t in_timestamp,
                                                               DisplayContentSample *aidl_return) {
  // getDisplayedContentSample is not supported
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayedContentSamplingAttributes(
    int64_t in_display, DisplayContentSamplingAttributes *aidl_return) {
  // getDisplayedContentSamplingAttributes is not supported
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::getDisplayPhysicalOrientation(int64_t in_display,
                                                                   Transform *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getHdrCapabilities(int64_t in_display,
                                                        HdrCapabilities *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getMaxVirtualDisplayCount(int32_t *aidl_return) {
  *aidl_return = -1;
  return ScopedAStatus::ok();
}

ScopedAStatus QtiQmaaComposerClient::getOverlaySupport(OverlayProperties *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getHdrConversionCapabilities(
    std::vector<HdrConversionCapability> *_aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setHdrConversionStrategy(
    const HdrConversionStrategy &in_conversionStrategy, Hdr *_aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setRefreshRateChangedCallbackDebugEnabled(int64_t in_display,
                                                                               bool in_enabled) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::getPerFrameMetadataKeys(
    int64_t in_display, std::vector<PerFrameMetadataKey> *aidl_return) {
  uint32_t count = (uint32_t)(PerFrameMetadataKey::MAX_FRAME_AVERAGE_LIGHT_LEVEL) + 1;
  aidl_return->resize(count);

  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getReadbackBufferAttributes(
    int64_t in_display, ReadbackBufferAttributes *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getReadbackBufferFence(
    int64_t in_display, ::ndk::ScopedFileDescriptor *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getRenderIntents(int64_t in_display, ColorMode in_mode,
                                                      std::vector<RenderIntent> *aidl_return) {
  uint32_t count = 1;
  std::vector<RenderIntent> intents;

  intents.resize(count);
  auto out_intents = reinterpret_cast<RenderIntent *>(
      reinterpret_cast<std::underlying_type<RenderIntent>::type *>(intents.data()));
  out_intents[0] = RenderIntent::COLORIMETRIC;

  *aidl_return = intents;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::getSupportedContentTypes(
    int64_t in_display, std::vector<ContentType> *aidl_return) {
  return ScopedAStatus::ok();
}

ScopedAStatus QtiQmaaComposerClient::getDisplayDecorationSupport(
    int64_t in_display, std::optional<DisplayDecorationSupport> *aidl_return) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::registerCallback(
    const std::shared_ptr<IComposerCallback> &in_callback) {
  callback_ = in_callback;
  OnHotplug(this, static_cast<int64_t>(DisplayConnectionType::INTERNAL), true);
  return ScopedAStatus::ok();
}

ScopedAStatus QtiQmaaComposerClient::setActiveConfig(int64_t in_display, int32_t in_config) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::setActiveConfigWithConstraints(
    int64_t in_display, int32_t in_config,
    const VsyncPeriodChangeConstraints &in_vsync_period_change_constraints,
    VsyncPeriodChangeTimeline *aidl_return) {
  VsyncPeriodChangeTimeline timeline;
  timeline.newVsyncAppliedTimeNanos = systemTime();
  timeline.refreshRequired = false;
  timeline.refreshTimeNanos = 0;

  *aidl_return = timeline;
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::setBootDisplayConfig(int64_t in_display, int32_t in_config) {
  // TODO: Add support in hwc_session
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::clearBootDisplayConfig(int64_t in_display) {
  // TODO: Add support in hwc_session
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::getPreferredBootDisplayConfig(int64_t in_display,
                                                                   int32_t *aidl_return) {
  // TODO: Add support in hwc_session
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setAutoLowLatencyMode(int64_t in_display, bool in_on) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setClientTargetSlotCount(int64_t in_display,
                                                              int32_t in_client_target_slot_count) {
  std::lock_guard<std::mutex> lock(m_display_data_mutex_);

  auto dpy = mDisplayData.find(in_display);
  if (dpy == mDisplayData.end()) {
    return TO_BINDER_STATUS(INT32(Error::BadDisplay));
  }
  dpy->second.ClientTargets.resize(in_client_target_slot_count);
  return ScopedAStatus::ok();
}

ScopedAStatus QtiQmaaComposerClient::setColorMode(int64_t in_display, ColorMode in_mode,
                                                  RenderIntent in_intent) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::setContentType(int64_t in_display, ContentType in_type) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setDisplayedContentSamplingEnabled(
    int64_t in_display, bool in_enable, FormatColorComponent in_component_mask,
    int64_t in_max_frames) {
  // setDisplayedContentSamplingEnabled is not supported
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

ScopedAStatus QtiQmaaComposerClient::setPowerMode(int64_t in_display, PowerMode in_mode) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::setReadbackBuffer(
    int64_t in_display, const NativeHandle &in_buffer,
    const ::ndk::ScopedFileDescriptor &in_release_fence) {
  return TO_BINDER_STATUS(INT32(Error::None));
}

ScopedAStatus QtiQmaaComposerClient::setVsyncEnabled(int64_t in_display, bool in_enabled) {
  return TO_BINDER_STATUS(INT32(Error::None));
}
ScopedAStatus QtiQmaaComposerClient::setIdleTimerEnabled(int64_t in_display, int32_t in_timeoutMs) {
  return TO_BINDER_STATUS(INT32(Error::Unsupported));
}

void QtiQmaaComposerClient::OnHotplug(void *callbackData, int64_t in_display, bool in_connected) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callbackData);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }
  if (in_connected) {
    std::lock_guard<std::mutex> lock_d(client->m_display_data_mutex_);
    client->mDisplayData.emplace(in_display, DisplayData(false));
  }

  client->callback_->onHotplug(in_display, in_connected);

  if (!in_connected) {
    // Wait for sufficient time to ensure sufficient resources are available to process connection.
    uint32_t vsync_period = 0;
    usleep(vsync_period * 2 / 1000);

    // Wait for the input command message queue to process before destroying the local display data.
    std::lock_guard<std::mutex> lock(client->m_command_mutex_);
    std::lock_guard<std::mutex> lock_d(client->m_display_data_mutex_);
    client->mDisplayData.erase(in_display);
  }
}

void QtiQmaaComposerClient::OnRefresh(void *callback_data, int64_t in_display) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callback_data);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }
  client->callback_->onRefresh(in_display);
  // hwc2_callback_data_t used here originally with a callback ret status log
}

void QtiQmaaComposerClient::OnSeamlessPossible(void *callback_data, int64_t in_display) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callback_data);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }

  client->callback_->onSeamlessPossible(in_display);
}

void QtiQmaaComposerClient::OnVsync(void *callback_data, int64_t in_display, int64_t in_timestamp,
                                    int32_t in_vsync_period_nanos) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callback_data);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }
  client->callback_->onVsync(in_display, in_timestamp, in_vsync_period_nanos);
  // hwc2_callback_data_t used here originally with a callback ret status log
}

void QtiQmaaComposerClient::OnVsyncPeriodTimingChanged(
    void *callback_data, int64_t in_display, const VsyncPeriodChangeTimeline &in_updated_timeline) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callback_data);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }
  client->callback_->onVsyncPeriodTimingChanged(in_display, in_updated_timeline);
  // hwc2_callback_data_t used here originally with a callback ret status log
}

void QtiQmaaComposerClient::OnVsyncIdle(void *callback_data, int64_t in_display) {
  auto client = reinterpret_cast<QtiQmaaComposerClient *>(callback_data);
  if (!client->callback_) {
    ALOGW("%s: Callback not registered or SF is unavailable.", __FUNCTION__);
    return;
  }
  client->callback_->onVsyncIdle(in_display);
}

Error QtiQmaaComposerClient::getDisplayReadbackBuffer(int64_t display,
                                                      const native_handle_t *rawHandle,
                                                      const native_handle_t **outHandle) {
  // TODO(user): revisit for caching and freeBuffer in success case.
  if (!mHandleImporter.importBuffer(rawHandle)) {
    ALOGE("%s: ImportBuffer failed.", __FUNCTION__);
    return Error::NoResources;
  }

  std::lock_guard<std::mutex> lock(m_display_data_mutex_);
  auto iter = mDisplayData.find(display);
  if (iter == mDisplayData.end()) {
    mHandleImporter.freeBuffer(rawHandle);
    return Error::BadDisplay;
  }

  *outHandle = rawHandle;
  return Error::None;
}

bool QtiQmaaComposerClient::CommandEngine::init() {
  mWriter = std::make_unique<ComposerServiceWriter>();
  return (mWriter != nullptr);
}

Error QtiQmaaComposerClient::CommandEngine::execute(const std::vector<DisplayCommand> &commands,
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

void QtiQmaaComposerClient::CommandEngine::executeSetColorTransform(
    int64_t display, const std::vector<float> &matrix) {}

void QtiQmaaComposerClient::CommandEngine::executeSetClientTarget(int64_t display,
                                                                  const ClientTarget &command) {
  bool useCache = !command.buffer.handle;
  buffer_handle_t clientTarget =
      useCache ? nullptr : ::android::makeFromAidl(*command.buffer.handle);
  native_handle_t *clientTargetClone = const_cast<native_handle_t *>(clientTarget);
  auto &sfd = const_cast<::ndk::ScopedFileDescriptor &>(command.buffer.fence);
  auto fd = sfd.get();
  *sfd.getR() = -1;

  auto err = lookupBuffer(display, -1, BufferCache::CLIENT_TARGETS, command.buffer.slot, useCache,
                          clientTarget, &clientTarget);
  if (err == Error::None) {
    auto updateBufErr = updateBuffer(display, -1, BufferCache::CLIENT_TARGETS, command.buffer.slot,
                                     useCache, clientTarget);
    if (err == Error::None) {
      err = updateBufErr;
    }
  }

  // Cleanup orginally cloned handle from the input
  native_handle_delete(clientTargetClone);

  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeSetDisplayBrightness(
    uint64_t display, const DisplayBrightness &command) {}

void QtiQmaaComposerClient::CommandEngine::executeSetOutputBuffer(uint64_t display,
                                                                  const Buffer &buffer) {
  bool useCache = !buffer.handle;
  buffer_handle_t outputBuffer = useCache ? nullptr : ::android::makeFromAidl(*buffer.handle);
  native_handle_t *outputBufferClone = const_cast<native_handle_t *>(outputBuffer);
  auto &sfd = const_cast<::ndk::ScopedFileDescriptor &>(buffer.fence);
  auto fd = sfd.get();
  *sfd.getR() = -1;

  auto err = lookupBuffer(display, -1, BufferCache::OUTPUT_BUFFERS, buffer.slot, useCache,
                          outputBuffer, &outputBuffer);
  if (err == Error::None) {
    auto updateBufErr =
        updateBuffer(display, -1, BufferCache::OUTPUT_BUFFERS, buffer.slot, useCache, outputBuffer);
    if (err == Error::None) {
      err = updateBufErr;
    }
  }

  // Cleanup orginally cloned handle from the input
  native_handle_delete(outputBufferClone);

  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeValidateDisplay(
    int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {
  executeSetExpectedPresentTimeInternal(display, expectedPresentTime);

  auto err = validateDisplay(display);

  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::getCapabilities() {
  uint32_t count = 2;
  int32_t *outCapabilities;
  std::vector<int32_t> composer_caps(count);

  outCapabilities = composer_caps.data();
  outCapabilities[0] = static_cast<int32_t>(Capability::SKIP_CLIENT_COLOR_TRANSFORM);
  outCapabilities[1] = static_cast<int32_t>(Capability::SKIP_VALIDATE);
  composer_caps.resize(count);

  mCapabilities.reserve(count);
  for (auto cap : composer_caps) {
    mCapabilities.insert(static_cast<Capability>(cap));
  }
}

void QtiQmaaComposerClient::CommandEngine::executePresentOrValidateDisplay(
    int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {
  executeSetExpectedPresentTimeInternal(display, expectedPresentTime);

  // First try to Present as is.
  mClient.getCapabilities();
  if (mClient.hasCapability(Capability::SKIP_VALIDATE)) {
    auto err = Error::None;
    if (err == Error::None) {
      mWriter->setPresentOrValidateResult(display, PresentOrValidate::Result::Presented);
      return;
    }
  }

  // Present has failed. We need to fallback to validate
  std::vector<int64_t> changedLayers;
  std::vector<Composition> compositionTypes;
  int32_t displayRequestMask = 0x0;
  std::vector<int64_t> requestedLayers;
  std::vector<int32_t> requestMasks;

  auto err = validateDisplay(display);
  if (err == Error::None) {
    mWriter->setPresentOrValidateResult(display, PresentOrValidate::Result::Validated);
    mWriter->setChangedCompositionTypes(display, changedLayers, compositionTypes);
    mWriter->setDisplayRequests(display, displayRequestMask, requestedLayers, requestMasks);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeAcceptDisplayChanges(int64_t display) {}

Error QtiQmaaComposerClient::CommandEngine::presentDisplay(int64_t display) {
  return Error::None;
}

void QtiQmaaComposerClient::CommandEngine::executePresentDisplay(int64_t display) {
  auto err = presentDisplay(display);
  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerCursorPosition(
    int64_t display, int64_t layer, const Point &cursorPosition) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerBuffer(int64_t display, int64_t layer,
                                                                 const Buffer &buffer) {
  bool useCache = !buffer.handle;
  buffer_handle_t layerBuffer = useCache ? nullptr : ::android::makeFromAidl(*buffer.handle);
  native_handle_t *layerBufferClone = const_cast<native_handle_t *>(layerBuffer);
  auto &sfd = const_cast<::ndk::ScopedFileDescriptor &>(buffer.fence);
  auto fd = sfd.get();
  *sfd.getR() = -1;
  auto error = lookupBuffer(display, layer, BufferCache::LAYER_BUFFERS, buffer.slot, useCache,
                            layerBuffer, &layerBuffer);
  if (error == Error::None) {
    auto updateBufErr = updateBuffer(display, layer, BufferCache::LAYER_BUFFERS, buffer.slot,
                                     useCache, layerBuffer);
    if (static_cast<Error>(error) == Error::None) {
      error = updateBufErr;
    }
  }

  // Cleanup orginally cloned handle from the input
  native_handle_delete(layerBufferClone);

  if (error != Error::None) {
    writeError(__FUNCTION__, error);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerSurfaceDamage(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &damage) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerBlendMode(
    int64_t display, int64_t layer, const ParcelableBlendMode &blendMode) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerColor(int64_t display, int64_t layer,
                                                                const FColor &color) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerComposition(
    int64_t display, int64_t layer, const ParcelableComposition &composition) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerDataspace(
    int64_t display, int64_t layer, const ParcelableDataspace &dataspace) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerDisplayFrame(int64_t display,
                                                                       int64_t layer,
                                                                       const Rect &rect) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerPlaneAlpha(int64_t display, int64_t layer,
                                                                     const PlaneAlpha &planeAlpha) {
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerSidebandStream(
    int64_t display, int64_t layer, const NativeHandle &sidebandStream) {
  // Sideband stream is not supported
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerSourceCrop(int64_t display, int64_t layer,
                                                                     const FRect &sourceCrop) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerTransform(
    int64_t display, int64_t layer, const ParcelableTransform &transform) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerVisibleRegion(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &visibleRegion) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerZOrder(int64_t display, int64_t layer,
                                                                 const ZOrder &zOrder) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerPerFrameMetadata(
    int64_t display, int64_t layer,
    const std::vector<std::optional<PerFrameMetadata>> &perFrameMetadata) {
  std::vector<int32_t> keys;
  std::vector<float> values;

  for (const auto &m : perFrameMetadata) {
    keys.push_back(INT32(m->key));
    values.push_back(static_cast<float>(m->value));
  }

  auto err = Error::None;
  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerColorTransform(
    int64_t display, int64_t layer, const std::vector<float> &colorTransform) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerPerFrameMetadataBlobs(
    int64_t display, int64_t layer,
    const std::vector<std::optional<PerFrameMetadataBlob>> &perFrameMetadataBlob) {
  std::vector<int32_t> keys;
  std::vector<uint32_t> sizes_of_metablob_;
  std::vector<uint8_t> blob_of_data_;

  for (const auto &m : perFrameMetadataBlob) {
    keys.push_back(INT32(m->key));
    sizes_of_metablob_.push_back(static_cast<uint32_t>(m->blob.size()));
    blob_of_data_.insert(blob_of_data_.end(), m->blob.begin(), m->blob.end());
  }

  auto err = Error::None;
  if (err != Error::None) {
    writeError(__FUNCTION__, err);
  }
}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerBrightness(
    int64_t display, int64_t layer, const LayerBrightness &brightness) {}

void QtiQmaaComposerClient::CommandEngine::executeSetExpectedPresentTimeInternal(
    int64_t display, const std::optional<ClockMonotonicTimestamp> expectedPresentTime) {}

void QtiQmaaComposerClient::CommandEngine::executeSetLayerBlockingRegion(
    int64_t display, int64_t layer, const std::vector<std::optional<Rect>> &blockingRegion) {
  // TODO: Add impl here and in hwc_session / hwc_display
  //   auto err = mClient.hwc_session_->SetLayerBlockingRegion(display, blockingRegion);
  //   if (err != Error::None) {
  //     writeError(__FUNCTION__, err);
  //   }
  // writeError(__FUNCTION__, Error::Unsupported);
}

Error QtiQmaaComposerClient::CommandEngine::validateDisplay(int64_t display) {
  return Error::None;
}

Error QtiQmaaComposerClient::CommandEngine::postPresentDisplay(int64_t display) {
  return Error::None;
}

Error QtiQmaaComposerClient::CommandEngine::postValidateDisplay(int64_t display,
                                                                uint32_t &types_count,
                                                                uint32_t &reqs_count) {
  return Error::None;
}

Error QtiQmaaComposerClient::CommandEngine::lookupBufferCacheEntryLocked(
    int64_t display, int64_t layer, BufferCache cache, uint32_t slot, BufferCacheEntry **outEntry) {
  auto dpy = mClient.mDisplayData.find(display);
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
      auto ly = dpy->second.Layers.find(layer);
      if (ly == dpy->second.Layers.end()) {
        return Error::BadLayer;
      }
      if (slot < ly->second.Buffers.size()) {
        entry = &ly->second.Buffers[slot];
      }
    } break;
    case BufferCache::LAYER_SIDEBAND_STREAMS: {
      auto ly = dpy->second.Layers.find(layer);
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
    ALOGW("%s: Invalid buffer slot %" PRIu32, __FUNCTION__, slot);
    return Error::BadParameter;
  }

  *outEntry = entry;

  return Error::None;
}

Error QtiQmaaComposerClient::CommandEngine::lookupBuffer(int64_t display, int64_t layer,
                                                         BufferCache cache, uint32_t slot,
                                                         bool useCache, buffer_handle_t handle,
                                                         buffer_handle_t *outHandle) {
  if (useCache) {
    std::lock_guard<std::mutex> lock(mClient.m_display_data_mutex_);

    BufferCacheEntry *entry;
    Error error = lookupBufferCacheEntryLocked(display, layer, cache, slot, &entry);
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

Error QtiQmaaComposerClient::CommandEngine::updateBuffer(int64_t display, int64_t layer,
                                                         BufferCache cache, uint32_t slot,
                                                         bool useCache, buffer_handle_t handle) {
  // handle was looked up from cache
  if (useCache) {
    return Error::None;
  }

  std::lock_guard<std::mutex> lock(mClient.m_display_data_mutex_);

  BufferCacheEntry *entry = nullptr;
  Error error = lookupBufferCacheEntryLocked(display, layer, cache, slot, &entry);
  if (error != Error::None) {
    return error;
  }

  *entry = handle;
  return Error::None;
}

SpAIBinder QtiQmaaComposerClient::createBinder() {
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
