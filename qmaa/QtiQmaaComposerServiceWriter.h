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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <aidl/android/hardware/graphics/composer3/CommandResultPayload.h>
#include <aidl/android/hardware/graphics/composer3/IComposerClient.h>
#include <inttypes.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

using aidl::android::hardware::graphics::composer3::ChangedCompositionLayer;
using aidl::android::hardware::graphics::composer3::ChangedCompositionTypes;
using aidl::android::hardware::graphics::composer3::ClientTargetProperty;
using aidl::android::hardware::graphics::composer3::ClientTargetPropertyWithBrightness;
using aidl::android::hardware::graphics::composer3::CommandError;
using aidl::android::hardware::graphics::composer3::CommandResultPayload;
using aidl::android::hardware::graphics::composer3::Composition;
using aidl::android::hardware::graphics::composer3::DimmingStage;
using aidl::android::hardware::graphics::composer3::DisplayRequest;
using aidl::android::hardware::graphics::composer3::PresentFence;
using aidl::android::hardware::graphics::composer3::PresentOrValidate;
using aidl::android::hardware::graphics::composer3::ReleaseFences;

class ComposerServiceWriter {
 public:
  ComposerServiceWriter() { reset(); }

  virtual ~ComposerServiceWriter() { reset(); }

  void reset() { mCommandsResults.clear(); }

  void setError(int32_t index, int32_t errorCode) {
    CommandError error;
    error.commandIndex = index;
    error.errorCode = errorCode;
    mCommandsResults.emplace_back(std::move(error));
  }

  void setPresentOrValidateResult(int64_t display, PresentOrValidate::Result result) {
    PresentOrValidate presentOrValidate;
    presentOrValidate.display = display;
    presentOrValidate.result = result;
    mCommandsResults.emplace_back(std::move(presentOrValidate));
  }

  void setChangedCompositionTypes(int64_t display, const std::vector<int64_t> &layers,
                                  const std::vector<Composition> &types) {
    ChangedCompositionTypes changedCompositionTypes;
    changedCompositionTypes.display = display;
    changedCompositionTypes.layers.reserve(layers.size());
    for (int i = 0; i < layers.size(); i++) {
      auto layer = ChangedCompositionLayer{.layer = layers[i], .composition = types[i]};
      changedCompositionTypes.layers.emplace_back(std::move(layer));
    }
    mCommandsResults.emplace_back(std::move(changedCompositionTypes));
  }

  void setDisplayRequests(int64_t display, int32_t displayRequestMask,
                          const std::vector<int64_t> &layers,
                          const std::vector<int32_t> &layerRequestMasks) {
    DisplayRequest displayRequest;
    displayRequest.display = display;
    displayRequest.mask = displayRequestMask;
    displayRequest.layerRequests.reserve(layers.size());
    for (int i = 0; i < layers.size(); i++) {
      auto layerRequest =
          DisplayRequest::LayerRequest{.layer = layers[i], .mask = layerRequestMasks[i]};
      displayRequest.layerRequests.emplace_back(std::move(layerRequest));
    }
    mCommandsResults.emplace_back(std::move(displayRequest));
  }

  void setPresentFence(int64_t display, ::ndk::ScopedFileDescriptor presentFence) {
    if (presentFence.get() >= 0) {
      PresentFence presentFenceCommand;
      presentFenceCommand.fence = std::move(presentFence);
      presentFenceCommand.display = display;
      mCommandsResults.emplace_back(std::move(presentFenceCommand));
    } else {
      ALOGV("%s: Invalid present fence %d", __FUNCTION__, presentFence.get());
    }
  }

  void setReleaseFences(int64_t display, const std::vector<int64_t> &layers,
                        std::vector<::ndk::ScopedFileDescriptor> releaseFences) {
    ReleaseFences releaseFencesCommand;
    releaseFencesCommand.display = display;
    for (int i = 0; i < layers.size(); i++) {
      if (releaseFences[i].get() >= 0) {
        ReleaseFences::Layer layer;
        layer.layer = layers[i];
        layer.fence = std::move(releaseFences[i]);
        releaseFencesCommand.layers.emplace_back(std::move(layer));
      } else {
        ALOGV("%s: Invalid release fence %d", __FUNCTION__, releaseFences[i].get());
      }
    }
    mCommandsResults.emplace_back(std::move(releaseFencesCommand));
  }

  void setClientTargetProperty(int64_t display, const ClientTargetProperty &clientTargetProperty,
                               float brightness, const DimmingStage &dimmingStage) {
    ClientTargetPropertyWithBrightness clientTargetPropertyWithBrightness;
    clientTargetPropertyWithBrightness.display = display;
    clientTargetPropertyWithBrightness.clientTargetProperty = clientTargetProperty;
    clientTargetPropertyWithBrightness.brightness = brightness;
    clientTargetPropertyWithBrightness.dimmingStage = dimmingStage;
    mCommandsResults.emplace_back(std::move(clientTargetPropertyWithBrightness));
  }

  std::vector<CommandResultPayload> getPendingCommandResults() {
    return std::move(mCommandsResults);
  }

 private:
  std::vector<CommandResultPayload> mCommandsResults;
};

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
