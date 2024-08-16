/*
 * Copyright (c) 2019-2024, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <log/log.h>

#include "QtiQmaaComposerHandleImporter.h"
#include <ui/GraphicBufferMapper.h>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

using ::android::GraphicBufferMapper;

ComposerHandleImporter::ComposerHandleImporter() : mInitialized(false) {}

void ComposerHandleImporter::initialize() {
  // allow only one client
  if (mInitialized) {
    return;
  }

  mInitialized = true;
  return;
}

void ComposerHandleImporter::cleanup() {
  mInitialized = false;
}

// In IComposer, any buffer_handle_t is owned by the caller and we need to
// make a clone for hwcomposer2.  We also need to translate empty handle
// to nullptr.  This function does that, in-place.
bool ComposerHandleImporter::importBuffer(buffer_handle_t &handle) {
  if (!handle) {
    return true;
  }

  if (!handle->numFds && !handle->numInts) {
    handle = nullptr;
    return true;
  }

  Mutex::Autolock lock(mLock);
  if (!mInitialized) {
    initialize();
  }

  buffer_handle_t importedHandle = nullptr;

  auto status = GraphicBufferMapper::get().importBufferNoValidate(handle, &importedHandle);

  if (status != ::android::OK) {
    ALOGE("%s: mapper importBuffer failed: %d", __FUNCTION__, status);
    return false;
  }

  handle = importedHandle;

  return true;
}

void ComposerHandleImporter::freeBuffer(buffer_handle_t handle) {
  if (!handle) {
    return;
  }

  Mutex::Autolock lock(mLock);

  auto status = GraphicBufferMapper::get().freeBuffer(handle);
  if (status != ::android::OK) {
    ALOGE("%s: mapper freeBuffer failed: %d", __FUNCTION__, status);
  }
}

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
