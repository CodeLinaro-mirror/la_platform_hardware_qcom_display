/*
 * Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <log/log.h>

#include "AidlComposerHandleImporter.h"
#include <cutils/properties.h>
#include "display_properties.h"
#include "QtiMapper5.h"
#include "gr_snap_helper.h"
#include "mapper_utils.h"

using mapper::GetMapperInstance;

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

ComposerHandleImporter::ComposerHandleImporter() : mInitialized(false) {}

void ComposerHandleImporter::initialize() {
  // allow only one client
  if (mInitialized) {
    return;
  }

  mMapper = GetMapperInstance();
  if (mMapper == nullptr) {
    ALOGE("%s: cannnot access QtiMapper5!", __FUNCTION__);
    return;
  }

  int value = 0;  // Default value when property is not present.
  value = property_get_bool("vendor.display.enable_memory_mapping", 0);
  enable_memory_mapping_ = (value == 1);

  mInitialized = true;
  return;
}

void ComposerHandleImporter::cleanup() {
  mMapper = nullptr;
  mInitialized = false;
}

void ComposerHandleImporter::InoFdMapInsert(int fd) {
  struct stat buf1;
  if (fstat(fd, &buf1)) {
    ALOGW("Fstat failed! fd=%d", fd);
    return;
  }
  uint64_t ino = (uint64_t)buf1.st_ino;
  ALOGV("insert fd=%d, ino=%lu", fd, ino);
  ino_fds_map_[ino].push_back(fd);
  if (ino_fds_map_.size() > MAX_INO_VALS) {
    ALOGW("ino allocation count=%lu", ino_fds_map_.size());
  }
}

void ComposerHandleImporter::InoFdMapRemove(int fd) {
  struct stat buf1;
  if (fstat(fd, &buf1)) {
    ALOGW("Fstat failed! fd=%d", fd);
    return;
  }
  uint64_t ino = (uint64_t)buf1.st_ino;
  std::vector<uint32_t> *fds = &ino_fds_map_[ino];
  auto it = std::find(fds->begin(), fds->end(), fd);
  if (it == fds->end()) {
    ALOGW("Ino value not found! Should not happen. ino=%lu, size=%lu", ino, fds->size());
    return;
  }
  ALOGV("remove fd=%d, ino=%lu", fd, ino);
  fds->erase(it);
  if (!ino_fds_map_[ino].size()) {
    ino_fds_map_.erase(ino);
  }
  if (ino_fds_map_.size() > MAX_INO_VALS) {
    ALOGW("allocation count=%lu", ino_fds_map_.size());
  }
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

  if (mMapper == nullptr) {
    ALOGE("%s: mMapper is null!", __FUNCTION__);
    return false;
  }

  buffer_handle_t importedHandle;

  auto ret = STABLEMAPPER(mMapper).importBuffer(handle, &importedHandle);

  if (ret != AIMAPPER_ERROR_NONE) {
    ALOGE("%s: mapper importBuffer failed: %d", __FUNCTION__, ret);
    return false;
  }

  handle = importedHandle;

  if (enable_memory_mapping_) {
    for (int i = 0; i < handle->numFds; i++) {
      // handle->data is the int array of fds. run insert on all fds.
      InoFdMapInsert(handle->data[i]);
    }
  }

  return true;
}

void ComposerHandleImporter::freeBuffer(buffer_handle_t handle) {
  if (!handle) {
    return;
  }

  Mutex::Autolock lock(mLock);

  if (mMapper == nullptr) {
    ALOGE("%s: mMapper is null!", __FUNCTION__);
    return;
  }

  if (enable_memory_mapping_) {
    for (int i = 0; i < handle->numFds; i++) {
      // handle->data is the int array of fds. run remove on all fds.
      InoFdMapRemove(handle->data[i]);
    }
  }

  auto ret = STABLEMAPPER(mMapper).freeBuffer(handle);
  if (ret != AIMAPPER_ERROR_NONE) {
    ALOGE("%s: mapper freeBuffer failed: %d", __FUNCTION__, ret);
  }
}

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
