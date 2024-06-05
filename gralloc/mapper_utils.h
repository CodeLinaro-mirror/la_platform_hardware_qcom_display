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
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <android/hardware/graphics/mapper/IMapper.h>
#include <aidl/android/hardware/graphics/allocator/IAllocator.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <dlfcn.h>
#include <vndksupport/linker.h>
#include <android/binder_manager.h>

#include "gr_buf_descriptor.h"

#define VENDOR_QTI_METADATA(type) \
  { mapper::VENDOR_QTI_METADATA_NAME, static_cast<int64_t>(type) }
#define STANDARD_METADATA(type) \
  { mapper::STANDARD_METADATA_NAME, static_cast<int64_t>(type) }
#define STABLEMAPPER(mapperptr) mapperptr->v5

using aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using android::hardware::graphics::mapper::StandardMetadata;

typedef AIMapper_Error (*AIMapper_loadIMapperFn)(AIMapper *_Nullable *_Nonnull outImplementation);

namespace mapper {

// Mapper utilities
constexpr const char *_Nonnull STANDARD_METADATA_NAME =
    "android.hardware.graphics.common.StandardMetadataType";
constexpr const char *_Nonnull VENDOR_QTI_METADATA_NAME = "QTI";
static bool isStandardMetadata(AIMapper_MetadataType metadataType) {
  return strcmp(STANDARD_METADATA_NAME, metadataType.name) == 0;
}
static bool isVendorMetadata(AIMapper_MetadataType metadataType) {
  return strcmp(VENDOR_QTI_METADATA_NAME, metadataType.name) == 0;
}

static AIMapper *_Nullable GetMapperInstance() {
  static AIMapper *mapper = nullptr;
  if (mapper) {
    ALOGI("Using previously loaded IMapper library.");
    return mapper;
  }

  // Obtain QTI IMapper library name
  std::string suffix = "qti";
  auto allocator_ =
      aidl::android::hardware::graphics::allocator::IAllocator::fromBinder(ndk::SpAIBinder(
          AServiceManager_checkService("android.hardware.graphics.allocator.IAllocator/default")));
  if (allocator_ == nullptr) {
    ALOGW("Unable to get allocator, using previously known IMapper library suffix 'qti'");
  } else {
    allocator_->getIMapperLibrarySuffix(&suffix);
  }
  std::string lib_name = "mapper." + suffix + ".so";

  void *so = android_load_sphal_library(lib_name.c_str(), RTLD_LOCAL | RTLD_NOW);
  if (!so) {
    ALOGE("Failed to load %s", lib_name.c_str());
    return nullptr;
  }

  auto loadIMapper = (AIMapper_loadIMapperFn)dlsym(so, "AIMapper_loadIMapper");
  AIMapper_Error error = loadIMapper(&mapper);
  if (error != AIMAPPER_ERROR_NONE) {
    ALOGE("AIMapper_loadIMapper failed %d", error);
    return nullptr;
  }
  auto mapper_version = (int32_t *)dlsym(so, "ANDROID_HAL_MAPPER_VERSION");
  // IMapper version check
  // When upgrading to a new stable version, update STABLEMAPPER macro above to point to correct
  // struct version
  if (mapper_version &&
      (*mapper_version != AIMAPPER_VERSION_5 || *mapper_version != (int32_t)mapper->version)) {
    ALOGE("IMapper version %d not equal to last known stable version %d, aborting IMapper init.",
          *mapper_version, AIMAPPER_VERSION_5);
    return nullptr;
  }
  return mapper;
}

template <aidl::android::hardware::graphics::common::StandardMetadataType T>
auto GetStandardMetadata(AIMapper *_Nonnull mapper_, buffer_handle_t _Nonnull buf_hnd)
    -> decltype(StandardMetadata<T>::value::decode(nullptr, 0)) {
  using Value = typename StandardMetadata<T>::value;
  std::vector<uint8_t> bytestream;
  bytestream.resize(128);

  auto size_required = STABLEMAPPER(mapper_).getStandardMetadata(
                       buf_hnd, static_cast<int64_t>(T), bytestream.data(), bytestream.size());
  if (size_required < 0) {
    ALOGW_IF(-AIMAPPER_ERROR_UNSUPPORTED != size_required,
             "%s: Unexpected error %d from valid getMetadata (%d) call", __FUNCTION__,
             -size_required, static_cast<int64_t>(T));
    return std::nullopt;
  }
  if ((size_t)size_required > bytestream.size()) {
    bytestream.resize(size_required);
    size_required = STABLEMAPPER(mapper_).getStandardMetadata(buf_hnd, static_cast<int64_t>(T),
                    bytestream.data(), bytestream.size());
  }
  if (size_required < 0 || (size_t)size_required > bytestream.size()) {
    ALOGW("getMetadata (%d) failed, received %d with buffer size %zd", static_cast<int64_t>(T),
           size_required, bytestream.size());
    return std::nullopt;
  }
  return Value::decode(bytestream.data(), size_required);
}

template <aidl::android::hardware::graphics::common::StandardMetadataType T>
AIMapper_Error SetStandardMetadata(AIMapper *_Nonnull mapper_, buffer_handle_t _Nonnull buf_hnd,
                                   const typename StandardMetadata<T>::value_type &value) {
  using Value = typename StandardMetadata<T>::value;
  std::vector<uint8_t> bytestream;

  auto size_required = Value::encode(value, nullptr, 0);
  if (size_required < 0) {
    ALOGW_IF(-AIMAPPER_ERROR_UNSUPPORTED != size_required,
             "%s: Unexpected error %d during size calculation for setMetadata (%d) call",
             __FUNCTION__, -size_required, static_cast<int64_t>(T));
    return static_cast<AIMapper_Error>(-size_required);
  }
  bytestream.resize(size_required);
  size_required = Value::encode(value, bytestream.data(), bytestream.size());
  if (size_required < 0 || (size_t)size_required > bytestream.size()) {
    ALOGW("setMetadata (%d) failed, calculated size %d with buffer size %zd",
          static_cast<int64_t>(T), size_required, bytestream.size());
    return static_cast<AIMapper_Error>(-size_required);
  }
  return STABLEMAPPER(mapper_).setStandardMetadata(buf_hnd, static_cast<int64_t>(T),
                                                   bytestream.data(), size_required);
}
}
