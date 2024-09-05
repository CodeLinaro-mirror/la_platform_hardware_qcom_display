/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 */

/*
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <gralloc_priv.h>
#include <QtiGralloc.h>

#include <gralloctypes/Gralloc4.h>
#include <core/buffer_allocator.h>
#include <utils/constants.h>
#include <utils/debug.h>

#include "gr_utils.h"
#include "hwc_buffer_allocator.h"
#include "hwc_debugger.h"
#include "hwc_layers.h"
#include "gr_snap_helper.h"
#include "mapper_utils.h"

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <android/rect.h>

#define __CLASS__ "HWCBufferAllocator"

#ifdef ENABLE_MAPPER_V5
using mapper::GetMapperInstance;
using mapper::GetMetadataState;
using mapper::GetStandardMetadata;
using mapper::GetVendorMetadata;
using mapper::IsSettable;
#endif
using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
#ifdef ENABLE_MAPPER_V5
using APixelFormat = aidl::android::hardware::graphics::common::PixelFormat;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
#else
using android::hardware::graphics::common::V1_2::PixelFormat;
using android::hardware::graphics::mapper::V4_0::BufferDescriptor;
#endif

#ifndef ENABLE_MAPPER_V5
using android::hardware::graphics::mapper::V4_0::Error;
#endif
using vendor::qti::hardware::display::mapperextensions::V1_0::PlaneLayout;
using MapperExtError = vendor::qti::hardware::display::mapperextensions::V1_0::Error;
#ifndef ENABLE_MAPPER_V5
using vendor::qti::hardware::display::mapper::V4_0::IQtiMapper;
#endif

using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::common::StandardMetadataType;
using ABufferUsage = aidl::android::hardware::graphics::common::BufferUsage;

namespace sdm {

#ifdef ENABLE_MAPPER_V5
  // Lazy initialization of gralloc HALs
int HWCBufferAllocator::GetGrallocInstance() {
#else
DisplayError HWCBufferAllocator::GetGrallocInstance() {
#endif
#ifdef ENABLE_MAPPER_V5
  if (mapper_ != nullptr && allocator_ != nullptr && snap_helper_ != nullptr) {
	return 0;
  }
#else
  if (mapper_ != nullptr || allocator_ != nullptr) {
    return kErrorNone;
  }
#endif

  allocator_ = IAllocator::fromBinder(ndk::SpAIBinder(
      AServiceManager_checkService("android.hardware.graphics.allocator.IAllocator/default")));
  if (allocator_ == nullptr) {
    DLOGE("Unable to get allocator");
    return -EINVAL; //kErrorCriticalResource;
  }

#ifdef ENABLE_MAPPER_V5
  //mapper_ = IMapper::getService(); //TBD BALDEV 
  if (mapper_ == nullptr) {
    mapper_ = GetMapperInstance();
    if (mapper_ == nullptr) {
      DLOGE("Unable to get mapper");
      return -EINVAL; //kErrorCriticalResource;
    }
  }

  if (snap_helper_ == nullptr) {
    snap_helper_ = gralloc::GrallocSnapHelper::GetInstance();
    if (snap_helper_ == nullptr) {
      DLOGW("Unable to get snap helper");
    }
  }

  return 0;
#else
  mapper_ = IMapper::getService();
  if (mapper_ == nullptr) {
    DLOGE("Unable to get mapper");
    return kErrorCriticalResource;
  }

  android::sp<IQtiMapper> qti_mapper = IQtiMapper::castFrom(mapper_);
  qti_mapper->getMapperExtensions([&](auto _error, auto _extensions) {
    if (_error == Error::NONE)
      mapper_ext_ = _extensions;
  });

  if (mapper_ext_ == nullptr) {
    DLOGE("Unable to get mapper extensions");
    return kErrorCriticalResource;
  }

  return kErrorNone;
#endif
}

static BufferDescriptorInfo CreateDescriptor(std::string name, uint32_t width, uint32_t height,
                                             int format, uint32_t layer_count, uint64_t usage) {
  BufferDescriptorInfo descriptorInfo{
      .width = static_cast<int32_t>(width),
      .height = static_cast<int32_t>(height),
      .layerCount = static_cast<int32_t>(layer_count),
      .format = static_cast<APixelFormat>(format),
      .usage = static_cast<ABufferUsage>(usage),
  };
  auto nameLength = std::min(name.length(), descriptorInfo.name.size() - 1);
  memcpy(descriptorInfo.name.data(), name.data(), nameLength);
  return descriptorInfo;
}

#ifdef ENABLE_MAPPER_V5
int HWCBufferAllocator::AllocateBuffer(BufferInfo *buffer_info) {
  int err = GetGrallocInstance();
  if (err != 0) {
    return err;
  }
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;
  BufferPermission buf_perm[BUFFER_CLIENT_MAX];
  int format;
  uint64_t alloc_flags = 0;
  int error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error != 0) {
    return -EINVAL;
  }

  if (buffer_config.access_control.empty()) {
    if (buffer_config.secure) {
      alloc_flags |= static_cast<uint64_t>(ABufferUsage::PROTECTED);
    }

    if (buffer_config.secure_camera) {
      alloc_flags |= static_cast<uint64_t>(ABufferUsage::CAMERA_OUTPUT);
    }

    if (!buffer_config.cache) {
      // Allocate uncached buffers
      alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
    }

    if (buffer_config.trusted_ui) {
      // Allocate cached buffers for trusted UI
      alloc_flags |= GRALLOC_USAGE_PRIVATE_TRUSTED_VM;
      alloc_flags &= ~GRALLOC_USAGE_PRIVATE_UNCACHED;
    }

    if (buffer_config.gfx_client) {
      alloc_flags |= static_cast<uint64_t>(ABufferUsage::GPU_TEXTURE);
    }

    alloc_flags |= static_cast<uint64_t>(ABufferUsage::COMPOSER_OVERLAY);
  } else {
    for (uint32_t i = 0; i < BUFFER_CLIENT_MAX; i++) {
      buf_perm[i].permission = 0;
    }
    auto it = buffer_config.access_control.find(kBufferClientUnTrustedVM);
    if (it != buffer_config.access_control.end()) {
      if (!buffer_config.cache) {
        // Allocate uncached buffers
        alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
      }
      SetBufferAccessControlInfo(it->second, &buf_perm[kBufferClientUnTrustedVM]);
    } else {
      alloc_flags |= static_cast<uint64_t>(ABufferUsage::PROTECTED);
    }

    it = buffer_config.access_control.find(kBufferClientTrustedVM);
    if (it != buffer_config.access_control.end()) {
      alloc_flags |= GRALLOC_USAGE_PRIVATE_TRUSTED_VM;
      SetBufferAccessControlInfo(it->second, &buf_perm[kBufferClientTrustedVM]);
    }

    it = buffer_config.access_control.find(kBufferClientDPU);
    if (it != buffer_config.access_control.end()) {
      alloc_flags |= static_cast<uint64_t>(ABufferUsage::COMPOSER_OVERLAY);
      SetBufferAccessControlInfo(it->second, &buf_perm[kBufferClientDPU]);
    }
  }

  buffer_handle_t buf = nullptr;

  BufferDescriptorInfo descriptor_info = CreateDescriptor(
      std::string("HWC_Buffer"), buffer_config.width, buffer_config.height, format, 1, alloc_flags);

  AllocationResult result;
  auto status = allocator_->allocate2(descriptor_info, 1, &result);
  if (!status.isOk()) {
    DLOGE("Failed to allocate buffer");
    return kErrorMemory;
  }
  native_handle *raw_handle = android::makeFromAidl(result.buffers[0]);

  auto mapper_err = STABLEMAPPER(mapper_).importBuffer(raw_handle, &buf);

  if (mapper_err != AIMAPPER_ERROR_NONE) {
    DLOGE("Failed to import buffer into HWC");
    return kErrorMemory;
  }

  uint32_t tmp_width;

  if (!buffer_config.access_control.empty()) {
    mapper_err = STABLEMAPPER(mapper_).setMetadata(
        buf, VENDOR_QTI_METADATA(SnapMetadataType::BUFFER_PERMISSION), buf_perm, sizeof(buf_perm));
    if (mapper_err != AIMAPPER_ERROR_NONE) {
      DLOGE("setMetadata failed for SnapMetadataType::BUFFER_PERMISSION %d", mapper_err);
      err = -EINVAL;
      goto cleanup;
    }
    auto error =
        GetMetadataValue(static_cast<void *>(raw_handle), SnapMetadataType::MEM_HANDLE,
                         &alloc_buffer_info->mem_handle, sizeof(alloc_buffer_info->mem_handle));
    if (error) {
      err = -EINVAL;
      goto cleanup;
    }
  }

  err = GetFd(raw_handle, alloc_buffer_info->fd);
  if (err != kErrorNone)
    goto cleanup;

  err = GetWidth(raw_handle, tmp_width);
  if (err != kErrorNone)
    goto cleanup;
  alloc_buffer_info->stride = tmp_width;
  alloc_buffer_info->aligned_width = tmp_width;

  err = GetHeight(raw_handle, alloc_buffer_info->aligned_height);
  if (err != kErrorNone)
    goto cleanup;

  err = GetAllocationSize(raw_handle, alloc_buffer_info->size);
  if (err != kErrorNone)
    goto cleanup;

  err = GetBufferId(raw_handle, alloc_buffer_info->id);
  if (err != kErrorNone)
    goto cleanup;

  err = GetSDMFormat(raw_handle, alloc_buffer_info->format);
  if (err != kErrorNone)
    goto cleanup;

  buffer_info->private_data = reinterpret_cast<void *>(raw_handle);
  return 0;

cleanup:
  if (buf) {
    STABLEMAPPER(mapper_).freeBuffer(buf);
  }
  return err;
}
#else
DisplayError HWCBufferAllocator::AllocateBuffer(BufferInfo *buffer_info) {
  auto err = GetGrallocInstance();
  if (err != kErrorNone) {
    return err;
  }
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  AllocatedBufferInfo *alloc_buffer_info = &buffer_info->alloc_buffer_info;
  int format;
  uint64_t alloc_flags = 0;
  int error = SetBufferInfo(buffer_config.format, &format, &alloc_flags);
  if (error != 0) {
    return kErrorParameters;
  }

  if (buffer_config.secure) {
    alloc_flags |= BufferUsage::PROTECTED;
  }

  if (buffer_config.secure_camera) {
    alloc_flags |= BufferUsage::CAMERA_OUTPUT;
  }

  if (!buffer_config.cache) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (buffer_config.gfx_client) {
    alloc_flags |= BufferUsage::GPU_TEXTURE;
  }

  alloc_flags |= BufferUsage::COMPOSER_OVERLAY;

  const native_handle_t *buf = nullptr;

  IMapper::BufferDescriptorInfo descriptor_info;
  descriptor_info.width = buffer_config.width;
  descriptor_info.height = buffer_config.height;
  descriptor_info.layerCount = 1;
  descriptor_info.format =
      static_cast<android::hardware::graphics::common::V1_2::PixelFormat>(format);
  descriptor_info.usage = alloc_flags;

  auto hidl_err = Error::NONE;

  auto descriptor = BufferDescriptor();
  mapper_->createDescriptor(descriptor_info, [&](const auto &_error, const auto &_descriptor) {
    hidl_err = _error;
    descriptor = _descriptor;
  });

  if (hidl_err != Error::NONE) {
    DLOGE("Failed to create descriptor");
    return kErrorMemory;
  }

  AllocationResult result;
  auto status = allocator_->allocate(descriptor, 1, &result);
  if (!status.isOk()) {
    DLOGE("Failed to allocate buffer");
    return kErrorMemory;
  }
  hidl_handle raw_handle = android::makeFromAidl(result.buffers[0]);

  mapper_->importBuffer(raw_handle, [&](const auto &_error, const auto &_buffer) {
    hidl_err = _error;
    buf = static_cast<const native_handle_t *>(_buffer);
  });

  if (hidl_err != Error::NONE) {
    DLOGE("Failed to import buffer into HWC");
    return kErrorMemory;
  }

  native_handle_t *hnd = nullptr;
  hnd = (native_handle_t *)buf;  // NOLINT

  err = GetFd(hnd, alloc_buffer_info->fd);
  if (err != kErrorNone)
    return kErrorUndefined;

  uint32_t tmp_width;
  err = GetWidth(hnd, tmp_width);
  if (err != kErrorNone)
    return kErrorUndefined;
  alloc_buffer_info->stride = tmp_width;
  alloc_buffer_info->aligned_width = tmp_width;

  err = GetHeight(hnd, alloc_buffer_info->aligned_height);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetAllocationSize(hnd, alloc_buffer_info->size);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetBufferId(hnd, alloc_buffer_info->id);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetSDMFormat(hnd, alloc_buffer_info->format);
  if (err != kErrorNone)
    return kErrorUndefined;

  buffer_info->private_data = reinterpret_cast<void *>(hnd);
  return kErrorNone;
}
#endif //ENABLE_MAPPER_V5

DisplayError HWCBufferAllocator::FreeBuffer(BufferInfo *buffer_info) {
  DisplayError err = kErrorNone;
#ifdef ENABLE_MAPPER_V5
  auto hnd = reinterpret_cast<buffer_handle_t>(buffer_info->private_data);
  STABLEMAPPER(mapper_).freeBuffer(hnd);
#else
  auto hnd = reinterpret_cast<void *>(buffer_info->private_data);
  mapper_->freeBuffer(hnd);
#endif

  AllocatedBufferInfo &alloc_buffer_info = buffer_info->alloc_buffer_info;

  alloc_buffer_info.fd = -1;
  alloc_buffer_info.stride = 0;
  alloc_buffer_info.size = 0;
  buffer_info->private_data = NULL;
  return err;
}

DisplayError HWCBufferAllocator::GetHeight(void *buf, uint32_t &height) {
  uint32_t tmp_height;
#ifdef ENABLE_MAPPER_V5
  auto err = STABLEMAPPER(mapper_).getMetadata(
      static_cast<buffer_handle_t>(buf),
      VENDOR_QTI_METADATA(SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS), &tmp_height,
      sizeof(tmp_height));
#else
  auto err = qtigralloc::get(buf, QTI_ALIGNED_HEIGHT_IN_PIXELS, &tmp_height);
#endif
  if (!err) {
    height = tmp_height;
    return kErrorNone;
  }
  return kErrorParameters;
}

DisplayError HWCBufferAllocator::GetWidth(void *buf, uint32_t &width) {
#ifdef ENABLE_MAPPER_V5
  auto result =
      GetStandardMetadata<StandardMetadataType::STRIDE>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    width = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
#else
  uint32_t tmp_width;
  auto err = qtigralloc::get(buf, QTI_ALIGNED_WIDTH_IN_PIXELS, &tmp_width);
  if (err == Error::NONE) {
    width = tmp_width;
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

int HWCBufferAllocator::GetUnalignedHeight(void *buf, uint32_t &height) {
#ifdef ENABLE_MAPPER_V5
  auto result =
      GetStandardMetadata<StandardMetadataType::HEIGHT>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    height = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
#else
  uint64_t tmp_height;
  auto err = Error::UNSUPPORTED;
  mapper_->get(
      buf, android::gralloc4::MetadataType_Height, [&](const auto _error, const auto _bytestream) {
        if (_error == Error::NONE)
          err = static_cast<Error>(android::gralloc4::decodeHeight(_bytestream, &tmp_height));
      });
  if (err == Error::NONE) {
    height = static_cast<uint32_t>(tmp_height);
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

int HWCBufferAllocator::GetUnalignedWidth(void *buf, uint32_t &width) {
#ifdef ENABLE_MAPPER_V5
  auto result =
      GetStandardMetadata<StandardMetadataType::WIDTH>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    width = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
#else
  uint64_t tmp_width;
  auto err = Error::UNSUPPORTED;
  mapper_->get(
      buf, android::gralloc4::MetadataType_Width, [&](const auto _error, const auto _bytestream) {
        if (_error == Error::NONE)
          err = static_cast<Error>(android::gralloc4::decodeWidth(_bytestream, &tmp_width));
      });
  if (err == Error::NONE) {
    width = static_cast<uint32_t>(tmp_width);
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

DisplayError HWCBufferAllocator::GetFd(void *buf, int &fd) {
  int tmp_fd;
#ifdef ENABLE_MAPPER_V5
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::FD), &tmp_fd,
                                               sizeof(tmp_fd));
  if (err >= 0) {
#else
  auto err = qtigralloc::get(buf, QTI_FD, &tmp_fd);
  if (err == Error::NONE) {
#endif
    fd = tmp_fd;
    return kErrorNone;
  }

  return kErrorParameters;
}

DisplayError HWCBufferAllocator::GetAllocationSize(void *buf, uint32_t &alloc_size) {
#ifdef ENABLE_MAPPER_V5
  auto result = GetStandardMetadata<StandardMetadataType::ALLOCATION_SIZE>(
      mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    alloc_size = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
#else
  uint64_t tmp_alloc_size;
  auto err = Error::UNSUPPORTED;
  mapper_->get(buf, android::gralloc4::MetadataType_AllocationSize,
               [&](const auto _error, const auto _bytestream) {
                 if (_error == Error::NONE)
                   err = static_cast<Error>(
                       android::gralloc4::decodeAllocationSize(_bytestream, &tmp_alloc_size));
               });
  if (err == Error::NONE) {
    alloc_size = static_cast<uint32_t>(tmp_alloc_size);
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

DisplayError HWCBufferAllocator::GetBufferId(void *buf, uint64_t &id) {
#ifdef ENABLE_MAPPER_V5
  auto result = GetStandardMetadata<StandardMetadataType::BUFFER_ID>(
      mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    id = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
#else
  uint64_t tmp_id;
  auto err = Error::UNSUPPORTED;
  mapper_->get(buf, android::gralloc4::MetadataType_BufferId,
               [&](const auto _error, const auto _bytestream) {
                 if (_error == Error::NONE)
                   err =
                       static_cast<Error>(android::gralloc4::decodeBufferId(_bytestream, &tmp_id));
               });
  if (err == Error::NONE) {
    id = tmp_id;
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

int HWCBufferAllocator::GetFormat(void *buf, int32_t &format) {
#ifdef ENABLE_MAPPER_V5
  int32_t ret_format;
  int err =
      GetVendorMetadata(mapper_, static_cast<buffer_handle_t>(buf),
                        SnapMetadataType::PIXEL_FORMAT_REQUESTED, &ret_format, sizeof(ret_format));

  if (err == AIMAPPER_ERROR_NONE) {
    format = ret_format;
    return kErrorNone;
  }
#else
  PixelFormat pixel_format;
  auto err = Error::UNSUPPORTED;
  mapper_->get(buf, android::gralloc4::MetadataType_PixelFormatRequested,
               [&](const auto _error, const auto _bytestream) {
                 if (_error == Error::NONE)
                   err = static_cast<Error>(
                       android::gralloc4::decodePixelFormatRequested(_bytestream, &pixel_format));
               });
  if (err == Error::NONE) {
    format = static_cast<int32_t>(pixel_format);
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

int HWCBufferAllocator::GetPrivateFlags(void *buf, int32_t &flags) {
#ifdef ENABLE_MAPPER_V5
  int64_t is_ubwc = 0, is_tile_rendered = 0, is_cached = 0;
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::IS_UBWC),
                                               &is_ubwc, sizeof(is_ubwc));
  err |= STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                           VENDOR_QTI_METADATA(SnapMetadataType::IS_TILE_RENDERED),
                                           &is_tile_rendered, sizeof(is_tile_rendered));
  err |= STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                           VENDOR_QTI_METADATA(SnapMetadataType::IS_CACHED),
                                           &is_cached, sizeof(is_cached));
  uint64_t buffer_usage;
  err |= STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                           VENDOR_QTI_METADATA(SnapMetadataType::USAGE),
                                           &buffer_usage, sizeof(buffer_usage));
  if (err >= 0) {
    // Private flags are being set here until pending changes to use snapalloc in SDM directly
    flags = is_ubwc ? (flags | qtigralloc::PRIV_FLAGS_UBWC_ALIGNED) : flags;
    flags = is_tile_rendered ? (flags | qtigralloc::PRIV_FLAGS_TILE_RENDERED) : flags;
    flags = is_cached ? (flags | qtigralloc::PRIV_FLAGS_CACHED) : flags;
    return kErrorNone;
  }
#else
  int32_t tmp_flags;
  auto err = qtigralloc::get(buf, QTI_PRIVATE_FLAGS, &tmp_flags);
  if (err == Error::NONE) {
    flags = tmp_flags;
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

DisplayError HWCBufferAllocator::GetSDMFormat(void *buf, LayerBufferFormat &sdm_format) {
  int32_t tmp_format, tmp_flags, err;
  err = GetFormat(buf, tmp_format);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetPrivateFlags(buf, tmp_flags);
  if (err != kErrorNone)
    return kErrorUndefined;

  sdm_format = HWCLayer::GetSDMFormat(tmp_format, tmp_flags);
  return kErrorNone;
}

int HWCBufferAllocator::GetBufferType(void *buf, uint32_t &buffer_type) {
  int32_t tmp_buffer_type;
#ifdef ENABLE_MAPPER_V5
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::BUFFER_TYPE),
                                               &tmp_buffer_type, sizeof(tmp_buffer_type));
  if (err >= 0) {
#else
  auto err = qtigralloc::get(buf, QTI_BUFFER_TYPE, &tmp_buffer_type);
  if (err == Error::NONE) {
#endif
    buffer_type = tmp_buffer_type;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetBufferGeometry(void *buf, int32_t &slice_width, int32_t &slice_height) {
#ifdef ENABLE_MAPPER_V5
  auto result =
      GetStandardMetadata<StandardMetadataType::CROP>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    slice_width = result.value()[0].right;
    slice_height = result.value()[0].bottom;
    return kErrorNone;
  }
#else
  auto err = Error::UNSUPPORTED;
  std::vector<aidl::android::hardware::graphics::common::Rect> tmp_crop;
  mapper_->get(buf, android::gralloc4::MetadataType_Crop,
               [&](const auto _error, const auto _bytestream) {
                 if (_error == Error::NONE)
                   err = static_cast<Error>(android::gralloc4::decodeCrop(_bytestream, &tmp_crop));
               });
  if (err == Error::NONE) {
    slice_width = tmp_crop[0].right;
    slice_height = tmp_crop[0].bottom;
    return kErrorNone;
  }
#endif
  return kErrorParameters;
}

#ifdef ENABLE_MAPPER_V5
int HWCBufferAllocator::GetCustomWidthAndHeight(const native_handle_t *handle, int *width,
                                                int *height) {
#else
void HWCBufferAllocator::GetCustomWidthAndHeight(const native_handle_t *handle, int *width,
                                                 int *height) {
#endif
  void *hnd = const_cast<native_handle_t *>(handle);

#ifdef ENABLE_MAPPER_V5
  GetMetadataValue(hnd, SnapMetadataType::STRIDE, width, sizeof(*width));
  GetMetadataValue(hnd, SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS, height, sizeof(*height));
#endif

  auto err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
  }
#ifdef ENABLE_MAPPER_V5
  int ret = 0;
  if (handle != nullptr) {
    if (snap_helper_->IsSnapAllocEnabled()) {
      ret = snap_helper_->GetMetadata(const_cast<native_handle_t *>(handle),
                                      SnapMetadataType::CUSTOM_DIMENSIONS_STRIDE, width,
                                      false);  // false -> convert_to_hidl_bytestream
      if (ret == 0) {
        ret = snap_helper_->GetMetadata(const_cast<native_handle_t *>(handle),
                                        SnapMetadataType::CUSTOM_DIMENSIONS_HEIGHT, height, false);
      }
    } else {
      ret = gralloc::GetCustomDimensions(static_cast<private_handle_t *>(hnd), width, height);
    }
    if (ret) {
      ALOGW(
          "%s: Error obtaining custom dimensions. "
          "stride: %d, height: %d",
          __FUNCTION__, *width, *height);
      return -EINVAL;
    }
  }
  return ret;
#else
  mapper_ext_->getCustomDimensions(hnd, [&](MapperExtError _error, auto _width, auto _height) {
    if (_error == MapperExtError::NONE) {
      *width = _width;
      *height = _height;
    }
  });
#endif
}

#ifdef ENABLE_MAPPER_V5
int HWCBufferAllocator::GetAlignedWidthAndHeight(int width, int height, int format,
                                                 uint32_t alloc_type, int *aligned_width,
                                                 int *aligned_height) {
#else
void HWCBufferAllocator::GetAlignedWidthAndHeight(int width, int height, int format,
                                                  uint32_t alloc_type, int *aligned_width,
                                                  int *aligned_height) {
#endif
  uint64_t usage = 0;
  if (alloc_type & GRALLOC_USAGE_HW_FB) {
    usage |= BufferUsage::COMPOSER_CLIENT_TARGET;
  }
  if (alloc_type & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    usage |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
  }
  *aligned_width = UINT(width);
  *aligned_height = UINT(height);

  auto err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
  }
#ifdef ENABLE_MAPPER_V5
  unsigned int alignedw, alignedh;
  *aligned_width = width;
  *aligned_height = height;

  if (snap_helper_->IsSnapAllocEnabled()) {
    uint64_t alignedw_ul = 0;
    uint64_t alignedh_ul = 0;
    gralloc::BufferDescriptor desc;
    desc.SetUsage(usage);
    desc.SetColorFormat(format);
    desc.SetDimensions(width, height);

    err = mapper::GetFromBufferDescriptor(mapper::ConvertGrallocToAidlDescriptor(desc),
                                          SnapMetadataType::ALIGNED_WIDTH_IN_PIXELS, &alignedw_ul,
                                          false);  // false -> convert_to_hidl_bytestream
    if (err == AIMAPPER_ERROR_NONE) {
      err = mapper::GetFromBufferDescriptor(mapper::ConvertGrallocToAidlDescriptor(desc),
                                            SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS,
                                            &alignedh_ul, false);
      alignedw = static_cast<unsigned int>(alignedw_ul);
      alignedh = static_cast<unsigned int>(alignedh_ul);
    }
  } else {
    gralloc::BufferInfo info(width, height, format, usage);
    err = gralloc::GetAlignedWidthAndHeight(info, &alignedw, &alignedh);
  }
  if (err) {
    return -EINVAL;
  } else {
    *aligned_width = static_cast<int>(alignedw);
    *aligned_height = static_cast<int>(alignedh);
  }

  return err;
#else
  mapper_ext_->calculateBufferAttributes(
      width, height, format, usage,
      [&](MapperExtError _error, auto _aligned_w, auto _aligned_h, auto _ubwc_enabled) {
        if (_error == MapperExtError::NONE) {
          *aligned_width = _aligned_w;
          *aligned_height = _aligned_h;
        }
      });
#endif
}

uint32_t HWCBufferAllocator::GetBufferSize(BufferInfo *buffer_info) {
#ifdef ENABLE_MAPPER_V5
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return 0;
  }
#endif
  const BufferConfig &buffer_config = buffer_info->buffer_config;
  uint64_t alloc_flags = GRALLOC_USAGE_PRIVATE_IOMMU_HEAP;

  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;
  uint32_t buffer_size;

  if (buffer_config.secure) {
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
  }

  if (!buffer_config.cache) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (SetBufferInfo(buffer_config.format, &format, &alloc_flags) < 0) {
    return 0;
  }

#ifdef ENABLE_MAPPER_V5
    uint32_t aligned_width = 0, aligned_height = 0;
    gralloc::BufferInfo info(static_cast<int>(width), static_cast<int>(height),
                             static_cast<int>(static_cast<APixelFormat>(format)), alloc_flags);
    if (gralloc::GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height) ==
        0) {
      return buffer_size;
    }
#else
  uint32_t aligned_width = 0, aligned_height = 0, buffer_size = 0;
  // TODO(user): Replace with getFromBufferDescriptorInfo
  gralloc::BufferInfo info(width, height, format, alloc_flags);
  int ret = GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height);
  if (ret < 0) {
    return 0;
  }
#endif
#ifdef ENABLE_MAPPER_V5
  return 0;
#else
  return buffer_size;
#endif
}

int HWCBufferAllocator::SetBufferInfo(LayerBufferFormat format, int *target, uint64_t *flags) {
  switch (format) {
    case kFormatRGBA8888:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      break;
    case kFormatRGBX8888:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      break;
    case kFormatRGB888:
      *target = HAL_PIXEL_FORMAT_RGB_888;
      break;
    case kFormatRGB565:
      *target = HAL_PIXEL_FORMAT_RGB_565;
      break;
    case kFormatBGR565:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      break;
    case kFormatBGR888:
      *target = HAL_PIXEL_FORMAT_BGR_888;
      break;
    case kFormatBGRA8888:
      *target = HAL_PIXEL_FORMAT_BGRA_8888;
      break;
    case kFormatYCrCb420PlanarStride16:
      *target = HAL_PIXEL_FORMAT_YV12;
      break;
    case kFormatYCrCb420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case kFormatYCbCr420SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP;
      break;
    case kFormatYCbCr422H2V1Packed:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_I;
      break;
    case kFormatCbYCrY422H2V1Packed:
      *target = HAL_PIXEL_FORMAT_CbYCrY_422_I;
      break;
    case kFormatYCbCr422H2V1SemiPlanar:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_SP;
      break;
    case kFormatYCbCr420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS;
      break;
    case kFormatYCrCb420SemiPlanarVenus:
      *target = HAL_PIXEL_FORMAT_YCrCb_420_SP_VENUS;
      break;
    case kFormatYCbCr420SPVenusUbwc:
    case kFormatYCbCr420SPVenusTile:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA5551:
      *target = HAL_PIXEL_FORMAT_RGBA_5551;
      break;
    case kFormatRGBA4444:
      *target = HAL_PIXEL_FORMAT_RGBA_4444;
      break;
    case kFormatRGBA1010102:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      break;
    case kFormatARGB2101010:
      *target = HAL_PIXEL_FORMAT_ARGB_2101010;
      break;
    case kFormatRGBX1010102:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      break;
    case kFormatXRGB2101010:
      *target = HAL_PIXEL_FORMAT_XRGB_2101010;
      break;
    case kFormatBGRA1010102:
      *target = HAL_PIXEL_FORMAT_BGRA_1010102;
      break;
    case kFormatABGR2101010:
      *target = HAL_PIXEL_FORMAT_ABGR_2101010;
      break;
    case kFormatBGRX1010102:
      *target = HAL_PIXEL_FORMAT_BGRX_1010102;
      break;
    case kFormatXBGR2101010:
      *target = HAL_PIXEL_FORMAT_XBGR_2101010;
      break;
    case kFormatYCbCr420P010:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_P010;
      break;
    case kFormatYCbCr420TP10Ubwc:
    case kFormatYCbCr420TP10Tile:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatYCbCr420P010Ubwc:
    case kFormatYCbCr420P010Tile:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_P010_UBWC;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatYCbCr420P010Venus:
      *target = HAL_PIXEL_FORMAT_YCbCr_420_P010_VENUS;
      break;
    case kFormatRGBA8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX8888Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_8888;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatBGR565Ubwc:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBA_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    default:
      DLOGE("Unsupported format = 0x%x", format);
      return -EINVAL;
  }
  return 0;
}

#ifdef ENABLE_MAPPER_V5
int HWCBufferAllocator::GetAllocatedBufferInfo(
    const BufferConfig &buffer_config, AllocatedBufferInfo *allocated_buffer_info) {
#else
DisplayError HWCBufferAllocator::GetAllocatedBufferInfo(
    const BufferConfig &buffer_config, AllocatedBufferInfo *allocated_buffer_info) {
#endif
  // TODO(user): This API should pass the buffer_info of the already allocated buffer
  // The private_data can then be typecast to the private_handle and used directly.
  uint64_t alloc_flags = GRALLOC_USAGE_PRIVATE_IOMMU_HEAP;

  int width = INT(buffer_config.width);
  int height = INT(buffer_config.height);
  int format;

  if (buffer_config.secure) {
    alloc_flags |= INT(GRALLOC_USAGE_PROTECTED);
  }

  if (!buffer_config.cache) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (SetBufferInfo(buffer_config.format, &format, &alloc_flags) < 0) {
    return kErrorParameters;
  }

#ifdef ENABLE_MAPPER_V5
  BufferInfo buffer_info = {buffer_config, *allocated_buffer_info};
  uint32_t buffer_size = 0;
  int aligned_width = 0, aligned_height = 0;
  int ret =
      GetAlignedWidthAndHeight(width, height, format, alloc_flags, &aligned_width, &aligned_height);
  buffer_size = GetBufferSize(&buffer_info);
  if (ret < 0 || buffer_size == 0) {
    return -EINVAL;
  }
#else
  uint32_t aligned_width = 0, aligned_height = 0, buffer_size = 0;
  // TODO(user): Replace with getFromBufferDescriptorInfo
  gralloc::BufferInfo info(width, height, format, alloc_flags);
  int ret = GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height);
  if (ret < 0) {
    return kErrorParameters;
  }
#endif
  allocated_buffer_info->stride = UINT32(aligned_width);
  allocated_buffer_info->aligned_width = UINT32(aligned_width);
  allocated_buffer_info->aligned_height = UINT32(aligned_height);
  allocated_buffer_info->size = UINT32(buffer_size);

  return kErrorNone;
}

#ifdef ENABLE_MAPPER_V5
DisplayError HWCBufferAllocator::GetBufferLayout(const AllocatedBufferInfo &buf_info, uint32_t stride[4],
                                        uint32_t offset[4], uint32_t *num_planes) {
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return kErrorParameters;
  }

  int format = static_cast<int>(APixelFormat::RGBA_8888);
  int plane_count = 0;
  uint64_t flags = 0;
  uint32_t size = 0;
  gralloc::PlaneLayoutInfo *plane_layout_info_ptr;
  SetBufferInfo(buf_info.format, &format, &flags);
  // Setup only the required stuff, skip rest
  if (flags & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    flags = qtigralloc::PRIV_FLAGS_UBWC_ALIGNED;
  }

  DLOGV("%s: Input parameters - wxh: %dx%d usage: 0x%" PRIu64 " format: %d", __FUNCTION__,
        buf_info.aligned_width, buf_info.aligned_height, buf_info.usage, format);

  gralloc::BufferInfo info(buf_info.aligned_width, buf_info.aligned_height, format, buf_info.usage);
  {
    unsigned int alignedw = 0, alignedh = 0;
    int custom_format = gralloc::GetImplDefinedFormat(buf_info.usage, format);
    err = gralloc::GetBufferSizeAndDimensions(info, &size, &alignedw, &alignedh);
    if (err) {
      return kErrorParameters;
    }

    gralloc::PlaneLayoutInfo plane_layout_info[8] = {};
    DLOGV("%s: Aligned width and height - wxh: %ux%u custom_format = %d", __FUNCTION__, alignedw,
          alignedh, custom_format);
    if (gralloc::IsYuvFormat(custom_format)) {
      // flags here only refers to layout (interlaced) flags, not private or buffer usage flags
      gralloc::GetYUVPlaneInfo(info, custom_format, alignedw, alignedh, flags, &plane_count,
                               plane_layout_info);
    } else if (gralloc::IsUncompressedRGBFormat(custom_format) ||
               gralloc::IsCompressedRGBFormat(custom_format)) {
      gralloc::GetRGBPlaneInfo(info, custom_format, alignedw, alignedh, flags, &plane_count,
                               plane_layout_info);
    } else {
      return kErrorParameters;
    }
    // We are only returning buffer layout for progressive or single field formats.
    *num_planes = (plane_count > 3) ? 2 : plane_count;

    for (int i = 0; i < *num_planes; i++) {
      offset[i] = static_cast<uint32_t>(plane_layout_info[i].offset);
      stride[i] = static_cast<uint32_t>(plane_layout_info[i].stride_bytes);
    }
    DLOGV("%s: Number of plane - %d, custom_format - %d", __FUNCTION__, plane_count, custom_format);
  }

  if (buf_info.format == kFormatYCrCb420PlanarStride16) {
    std::swap(offset[1], offset[2]);
  }

  if (flags & qtigralloc::PRIV_FLAGS_UBWC_ALIGNED) {
    std::fill(offset, offset + 4, 0);
  }

  return kErrorNone;
}
#else
DisplayError HWCBufferAllocator::GetBufferLayout(const AllocatedBufferInfo &buf_info,
                                                 uint32_t stride[4], uint32_t offset[4],
                                                 uint32_t *num_planes) {
// TODO(user): Transition APIs to not need a private handle
  private_handle_t hnd(-1, 0, 0, 0, 0, 0, 0);
  int format = HAL_PIXEL_FORMAT_RGBA_8888;
  uint64_t flags = 0;

  SetBufferInfo(buf_info.format, &format, &flags);
  // Setup only the required stuff, skip rest
  hnd.format = format;
  hnd.width = INT32(buf_info.aligned_width);
  hnd.height = INT32(buf_info.aligned_height);
  if (flags & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    hnd.flags = private_handle_t::PRIV_FLAGS_UBWC_ALIGNED;
  }

  int ret = gralloc::GetBufferLayout(&hnd, stride, offset, num_planes);
  if (ret < 0) {
    DLOGE("GetBufferLayout failed");
    return kErrorParameters;
  }

  return kErrorNone;
}
#endif

int HWCBufferAllocator::MapBuffer(const native_handle_t *handle, int acquire_fence,
                                  void **base_ptr) {
  auto err = GetGrallocInstance();
  if (err != kErrorNone) {
    return err;
  }

  if (base_ptr == NULL) {
      DLOGE("base ptr is NULL");
      return kErrorParameters;
  }
  NATIVE_HANDLE_DECLARE_STORAGE(acquire_fence_storage, 1, 0);
  hidl_handle acquire_fence_handle;
  if (acquire_fence >= 0) {
    auto h = native_handle_init(acquire_fence_storage, 1, 0);
    if (!h) {
      DLOGE("native_handle_init failed");
      return kErrorParameters;
    }
    h->data[0] = acquire_fence;
    acquire_fence_handle = h;
  }

#ifdef ENABLE_MAPPER_V5
  auto hnd = static_cast<buffer_handle_t>(const_cast<native_handle_t *>(handle));
  *base_ptr = NULL;
  ARect access_region = {.left = 0, .top = 0, .right = 0, .bottom = 0};
  auto error = STABLEMAPPER(mapper_).lock(hnd, (uint64_t)ABufferUsage::CPU_READ_OFTEN,
                                          access_region, acquire_fence, base_ptr);

  if (!*base_ptr || error != AIMAPPER_ERROR_NONE) {
    DLOGW("*base_ptr is NULL! lock(%p, ...) failed: %d", hnd, error);
    return kErrorUndefined;
  }
#else
  auto hnd = const_cast<native_handle_t *>(handle);
  *base_ptr = NULL;
  const IMapper::Rect access_region = {.left = 0, .top = 0, .width = 0, .height = 0};
  mapper_->lock(reinterpret_cast<void *>(hnd), (uint64_t)BufferUsage::CPU_READ_OFTEN, access_region,
                acquire_fence_handle, [&](const auto &_error, const auto &_buffer) {
                  if (_error == Error::NONE) {
                    *base_ptr = _buffer;
                  }
                });

  if (!*base_ptr) {
    DLOGE("buffer is NULL");
    return kErrorUndefined;
  }
#endif

  return kErrorNone;
}

DisplayError HWCBufferAllocator::UnmapBuffer(const native_handle_t *handle, int *release_fence) {
  DisplayError err = kErrorNone;
  *release_fence = -1;
#ifdef ENABLE_MAPPER_V5
  auto hnd = static_cast<buffer_handle_t>(const_cast<native_handle_t *>(handle));
  auto error = STABLEMAPPER(mapper_).unlock(hnd, release_fence);

  if (error != AIMAPPER_ERROR_NONE) {
    ALOGW("unlock failed with error %d", error);
    err = kErrorUndefined;
  }
#else
  auto hnd = const_cast<native_handle_t *>(handle);
  mapper_->unlock(reinterpret_cast<void *>(hnd),
                  [&](const auto &_error, const auto &_release_fence) {
                    if (_error != Error::NONE) {
                      err = kErrorUndefined;
                    }
                  });
#endif
  return err;
}

void HWCBufferAllocator::SetBufferAccessControlInfo(std::bitset<kBufferPermMax> permission,
                                                    BufferPermission *buf_perm) {
  buf_perm->read = permission.test(kBufferPermRead);
  buf_perm->write = permission.test(kBufferPermWrite);
  buf_perm->execute = permission.test(kBufferPermExecute);
}

int HWCBufferAllocator::GetCustomContentMetadata(void *buf, CustomContentMetadata *dest) {
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
  }

  if (!buf || !dest) {
    err = -EINVAL;
  } else {
    auto error = STABLEMAPPER(mapper_).getMetadata(
        static_cast<buffer_handle_t>(buf),
        VENDOR_QTI_METADATA(SnapMetadataType::CUSTOM_CONTENT_METADATA), static_cast<void *>(dest),
        sizeof(*dest));
    if (error < 0) {
      err = -ENOTSUP;
    }
  }

  return err;
}

int HWCBufferAllocator::GetMetadataValue(void *buf, SnapMetadataType type, void *dest,
                                         size_t dest_size) {
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
  }

  if (!buf || !dest) {
    err = -EINVAL;
  } else {
    bool metadata_set = true;
    AIMapper_Error error = AIMAPPER_ERROR_NONE;

    if (IsSettable(mapper_, type)) {
      error = GetMetadataState(static_cast<buffer_handle_t>(buf), type, &metadata_set);
      DLOGE(">>> Metadata type (SnapMetadatatype) %ld state is %d (set = true)",
            type, metadata_set);
    }
    if (metadata_set) {
      error = GetVendorMetadata(mapper_, static_cast<buffer_handle_t>(buf), type, dest, dest_size);
      DLOGE(">>> GetVendorMetadata() called with type(%ld) and "\
            "dest_size %d and returns err %d.\n", type, dest_size, error);
    }

    if (error != AIMAPPER_ERROR_NONE || !metadata_set) {
      err = -ENOTSUP;
    }
  }

  return err;
}
}  // namespace sdm
