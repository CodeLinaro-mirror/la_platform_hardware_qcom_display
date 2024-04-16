/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>
#include <cutils/native_handle.h>
#include <dlfcn.h>
#include <vndksupport/linker.h>

#include <QtiGralloc.h>
#include <gralloctypes/Gralloc4.h>

#include <algorithm>
#include <string>

#include "gr_buf_mgr.h"
#include "mapper_utils.h"
namespace stablec {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace mapper5 {

using namespace ::aidl::android::hardware::graphics::common;
using namespace ::android::hardware::graphics::mapper;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using ::android::base::unique_fd;
using Error = AIMapper_Error;
using GrallocPixelFormat = aidl::android::hardware::graphics::common::PixelFormat;
using gralloc::BufferManager;
using GrallocExtendableType = aidl::android::hardware::graphics::common::ExtendableType;
using GrallocDataspace = aidl::android::hardware::graphics::common::Dataspace;
using mapper::isStandardMetadata;
using mapper::isVendorMetadata;
using mapper::STANDARD_METADATA_NAME;
using mapper::VENDOR_QTI_METADATA_NAME;


#define REQUIRE_DRIVER()                                       \
  ALOGE("Failed to %s. Driver is uninitialized.", __func__); \
  return AIMAPPER_ERROR_NO_RESOURCES;                        \

#define VALIDATE_BUFFER_HANDLE(bufferHandle)                \
  if (!(bufferHandle)) {                                    \
    ALOGW("Failed to %s. Null buffer_handle_t.", __func__); \
    return AIMAPPER_ERROR_BAD_BUFFER;                       \
  }

constexpr unsigned int METADATA_BUFFERSIZE_INITIAL = 10000;

#define VALIDATE_DRIVER_AND_BUFFER_HANDLE(bufferHandle) \
  REQUIRE_DRIVER()                                      \
  VALIDATE_BUFFER_HANDLE(bufferHandle)

class QtiMapper5Legacy final : public ::vendor::mapper::IMapperV5Impl {
 public:
  QtiMapper5Legacy();
  ~QtiMapper5Legacy() override = default;
  Error importBuffer(const native_handle_t *_Nonnull handle,
                     buffer_handle_t _Nullable *_Nonnull outBufferHandle) override;
  Error freeBuffer(buffer_handle_t _Nonnull buffer) override;
  Error getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t *_Nonnull outNumFds,
                         uint32_t *_Nonnull outNumInts) override;
  Error lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
             int acquireFence, void *_Nullable *_Nonnull outData) override;
  Error unlock(buffer_handle_t _Nonnull buffer, int *_Nonnull releaseFence) override;
  Error flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;
  Error rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;
  int32_t getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
                      void *_Nonnull outData, size_t outDataSize) override;
  int32_t getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
                              void *_Nonnull outData, size_t outDataSize) override;
  Error setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
                    const void *_Nonnull metadata, size_t metadataSize) override;
  Error setStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
                            const void *_Nonnull metadata, size_t metadataSize) override;
  Error listSupportedMetadataTypes(
      const AIMapper_MetadataTypeDescription *_Nullable *_Nonnull outDescriptionList,
      size_t *_Nonnull outNumberOfDescriptions) override;
  Error dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
                   AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
                   void *_Null_unspecified context) override;
  Error dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
                       AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
                       void *_Null_unspecified context) override;
  Error getReservedRegion(buffer_handle_t _Nonnull buffer,
                          void *_Nullable *_Nonnull outReservedRegion,
                          uint64_t *_Nonnull outReservedSize) override;

 private:
  void WaitFenceFd(int fence_fd);
  Error DumpBufferMetadata(buffer_handle_t _Nonnull buffer,
                           AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
                           void *_Null_unspecified context);
  int32_t GetMetadataPrivate(buffer_handle_t _Nonnull bufferHandle, int64_t metadataType,
                             void *_Nonnull outData, size_t outDataSize, bool isStandard);
  Error SetMetadataPrivate(buffer_handle_t _Nonnull bufferHandle, int64_t metadataType,
                           const void *_Nonnull metadata, size_t metadataSize, bool isStandard);

  BufferManager *_Nullable buf_mgr_ = nullptr;

  std::unordered_map<uint64_t, size_t> type_to_size_{
      {static_cast<uint64_t>(StandardMetadataType::BUFFER_ID), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::NAME), sizeof(std::string)},
      {static_cast<uint64_t>(StandardMetadataType::WIDTH), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::HEIGHT), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::LAYER_COUNT), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::PIXEL_FORMAT_REQUESTED),
       sizeof(GrallocPixelFormat)},
      {static_cast<uint64_t>(StandardMetadataType::PIXEL_FORMAT_FOURCC), sizeof(uint32_t)},
      {static_cast<uint64_t>(StandardMetadataType::PIXEL_FORMAT_MODIFIER), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::USAGE), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::ALLOCATION_SIZE), sizeof(uint32_t)},
      {static_cast<uint64_t>(StandardMetadataType::PROTECTED_CONTENT), sizeof(uint64_t)},
      {static_cast<uint64_t>(StandardMetadataType::COMPRESSION), sizeof(GrallocExtendableType)},
      {static_cast<uint64_t>(StandardMetadataType::INTERLACED), sizeof(GrallocExtendableType)},
      {static_cast<uint64_t>(StandardMetadataType::CHROMA_SITING), sizeof(GrallocExtendableType)},
      {static_cast<uint64_t>(StandardMetadataType::PLANE_LAYOUTS), sizeof(sdm::BufferLayout)},
      {static_cast<uint64_t>(StandardMetadataType::CROP), sizeof(Rect)},
      {static_cast<uint64_t>(StandardMetadataType::DATASPACE), sizeof(GrallocDataspace)},
      {static_cast<uint64_t>(StandardMetadataType::BLEND_MODE), sizeof(BlendMode)},
      {static_cast<uint64_t>(QTI_COLOR_METADATA), sizeof(ColorMetaData)},
      {static_cast<uint64_t>(QTI_PRIVATE_FLAGS), sizeof(int32_t)},
      {static_cast<uint64_t>(QTI_COLORSPACE), sizeof(uint32_t)},
      {static_cast<uint64_t>(QTI_YUV_PLANE_INFO), (YCBCR_LAYOUT_ARRAY_SIZE * sizeof(qti_ycbcr))},
      // TODO: Add support for missing StandardMetadataType
  };
};

}  // namespace mapper5
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace stablec
