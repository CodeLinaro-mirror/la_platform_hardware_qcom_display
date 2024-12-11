/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
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
#include <QtiGralloc.h>

#include <gralloctypes/Gralloc4.h>
#include <core/buffer_allocator.h>
#include <utils/constants.h>
#include <utils/debug.h>
#include <gr_utils.h>
#include <BufferUsage.h>

#include "hwc_buffer_allocator.h"
#include "gr_snap_helper.h"
#include "mapper_utils.h"

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <android/rect.h>
#include "sdm_interface_factory.h"
#include "sdm_display_intf_layer_builder.h"
#include "hwc_common.h"

#include <BufferUsage.h>
#include <PixelFormat.h>
#include <AllocationResult.h>

#define __CLASS__ "HWCBufferAllocator"

using mapper::GetMapperInstance;
using mapper::GetMetadataState;
using mapper::GetStandardMetadata;
using mapper::GetVendorMetadata;
using mapper::IsSettable;

using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using APixelFormat = aidl::android::hardware::graphics::common::PixelFormat;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::common::StandardMetadataType;
using ABufferUsage = aidl::android::hardware::graphics::common::BufferUsage;

using SnapAllocationResult = vendor::qti::hardware::display::snapalloc::AllocationResult;
using SnapBufferDescriptor = vendor::qti::hardware::display::snapalloc::BufferDescriptor;
using SnapError = vendor::qti::hardware::display::snapalloc::Error;
using SnapHandle = vendor::qti::hardware::display::snapalloc::SnapHandle;

using SnapBufferUsage = vendor_qti_hardware_display_common_BufferUsage;
using SnapMetadataType = vendor_qti_hardware_display_common_MetadataType;
using SnapPixelFormat = vendor_qti_hardware_display_common_PixelFormat;

namespace sdm {
int HWCBufferAllocator::GetGrallocInstance() {
  // Lazy initialization of gralloc HALs
  if (mapper_ != nullptr && allocator_ != nullptr && snap_helper_ != nullptr) {
    return kErrorNone;
  }

  if (allocator_ == nullptr) {
    allocator_ = IAllocator::fromBinder(ndk::SpAIBinder(
        AServiceManager_checkService("android.hardware.graphics.allocator.IAllocator/default")));
    if (allocator_ == nullptr) {
      DLOGE("Unable to get allocator");
      return kErrorCriticalResource;
    }
  }

  if (mapper_ == nullptr) {
    mapper_ = GetMapperInstance();
    if (mapper_ == nullptr) {
      DLOGE("Unable to get mapper");
      return kErrorCriticalResource;
    }
  }

  if (snap_helper_ == nullptr) {
    snap_helper_ = gralloc::GrallocSnapHelper::GetInstance();
    if (snap_helper_ == nullptr) {
      DLOGW("Unable to get snap helper");
    }
  }

  return 0;
}

int HWCBufferAllocator::GetSnapInstance() {
  if (snapmapper_ != nullptr && snapallocator_ != nullptr) {
    return 0;
  }

  const std::string snapalloc_lib_name = "vendor.qti.hardware.display.snapalloc-impl.so";
  void *snap_impl_lib_ = ::dlopen(snapalloc_lib_name.c_str(), RTLD_NOW);
  if (!snap_impl_lib_) {
    ALOGE("Dlopen error for snapalloc impl: %s", dlerror());
    return kErrorCriticalResource;
  }

  std::shared_ptr<ISnapAlloc> (*LINK_FETCH_ISnapAlloc)(DebugCallbackIntf *) = nullptr;
  *reinterpret_cast<void **>(&LINK_FETCH_ISnapAlloc) = ::dlsym(snap_impl_lib_, "FETCH_ISnapAlloc");
  if (LINK_FETCH_ISnapAlloc) {
    snapallocator_ = LINK_FETCH_ISnapAlloc(nullptr);
  }

  if (snapallocator_ == nullptr) {
    ALOGE("%s: Failed to link FETCH_ISnapAlloc - %s", __FUNCTION__, strerror(errno));
    return kErrorCriticalResource;
  }

  std::shared_ptr<ISnapMapper> (*LINK_FETCH_ISnapMapper)(DebugCallbackIntf *) = nullptr;
  *reinterpret_cast<void **>(&LINK_FETCH_ISnapMapper) =
      ::dlsym(snap_impl_lib_, "FETCH_ISnapMapper");
  if (LINK_FETCH_ISnapMapper) {
    snapmapper_ = LINK_FETCH_ISnapMapper(nullptr);
  }

  if (snapmapper_ == nullptr) {
    ALOGE("%s: Failed to link FETCH_ISnapMapper - %s", __FUNCTION__, strerror(errno));
    return kErrorCriticalResource;
  }

  return 0;
}

static SnapBufferDescriptor CreateDescriptor(std::string name, uint32_t width, uint32_t height,
                                             int format, uint32_t layer_count, uint64_t usage) {
  SnapBufferDescriptor descriptorInfo{
      .width = static_cast<int32_t>(width),
      .height = static_cast<int32_t>(height),
      .layerCount = static_cast<int32_t>(layer_count),
      .format = static_cast<SnapPixelFormat>(format),
      .usage = static_cast<SnapBufferUsage>(usage),
  };
  auto nameLength = std::min(name.length(), static_cast<size_t>(MAX_NAME_LEN - 1));
  memcpy(descriptorInfo.name, name.data(), nameLength);
  return descriptorInfo;
}

int HWCBufferAllocator::AllocateBuffer(BufferInfo *buffer_info) {
  auto err = GetSnapInstance();
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
      alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::PROTECTED);
    }

    if (buffer_config.secure_camera) {
      alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::CAMERA_OUTPUT);
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
      alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::GPU_TEXTURE);
    }

    alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::COMPOSER_OVERLAY);
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
      alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::PROTECTED);
    }

    it = buffer_config.access_control.find(kBufferClientTrustedVM);
    if (it != buffer_config.access_control.end()) {
      alloc_flags |= GRALLOC_USAGE_PRIVATE_TRUSTED_VM;
      SetBufferAccessControlInfo(it->second, &buf_perm[kBufferClientTrustedVM]);
    }

    it = buffer_config.access_control.find(kBufferClientDPU);
    if (it != buffer_config.access_control.end()) {
      alloc_flags |= static_cast<uint64_t>(SnapBufferUsage::COMPOSER_OVERLAY);
      SetBufferAccessControlInfo(it->second, &buf_perm[kBufferClientDPU]);
    }
  }

  SnapBufferDescriptor descriptor_info = CreateDescriptor(
      std::string("HWC_Buffer"), buffer_config.width, buffer_config.height, format, 1, alloc_flags);

  SnapAllocationResult result;
  auto status = snapallocator_->Allocate(descriptor_info, 1, &result);
  if (status != SnapError::NONE) {
    DLOGE("Failed to allocate buffer: %d", status);
    return kErrorMemory;
  }
  SnapHandle *handle = result.handles[0];

  uint32_t tmp_width;

  if (!buffer_config.access_control.empty()) {
    status = snapmapper_->SetMetadata(*handle, SnapMetadataType::BUFFER_PERMISSION, buf_perm);
    if (status != SnapError::NONE) {
      DLOGE("setMetadata failed for SnapMetadataType::BUFFER_PERMISSION %d", status);
      err = -EINVAL;
      goto cleanup;
    }
    status = snapmapper_->GetMetadata(*handle, SnapMetadataType::MEM_HANDLE,
                                      &alloc_buffer_info->mem_handle);
    if (error != SnapError::NONE) {
      err = -EINVAL;
      goto cleanup;
    }
  }

  status = snapmapper_->GetMetadata(*handle, SnapMetadataType::FD, &alloc_buffer_info->fd);
  if (status != SnapError::NONE)
    goto cleanup;

  status = snapmapper_->GetMetadata(*handle, SnapMetadataType::STRIDE, &tmp_width);
  if (status != SnapError::NONE)
    goto cleanup;
  alloc_buffer_info->stride = tmp_width;
  alloc_buffer_info->aligned_width = tmp_width;

  status = snapmapper_->GetMetadata(*handle, SnapMetadataType::HEIGHT,
                                    &alloc_buffer_info->aligned_height);
  if (status != SnapError::NONE)
    goto cleanup;

  status = snapmapper_->GetMetadata(*handle, SnapMetadataType::ALLOCATION_SIZE,
                                    &alloc_buffer_info->size);
  if (status != SnapError::NONE)
    goto cleanup;

  status = snapmapper_->GetMetadata(*handle, SnapMetadataType::BUFFER_ID, &alloc_buffer_info->id);
  if (status != SnapError::NONE)
    goto cleanup;

  int64_t tmp_format, is_ubwc, compression_type;
  snapmapper_->GetMetadata(*handle, SnapMetadataType::PIXEL_FORMAT_ALLOCATED, &tmp_format);
  snapmapper_->GetMetadata(*handle, SnapMetadataType::IS_UBWC, &is_ubwc);
  snapmapper_->GetMetadata(*handle, SnapMetadataType::COMPRESSION, &compression_type);

  int32_t flag;
  flag = INT32(is_ubwc ? SnapMetadataType::IS_UBWC : 0);
  alloc_buffer_info->format = GetSDMFormat(tmp_format, flag, compression_type);

  buffer_info->private_data = (void *)handle;
  return 0;

cleanup:
  if (handle) {
    snapmapper_->Release(*handle);
  }
  return err;
}

int HWCBufferAllocator::FreeBuffer(BufferInfo *buffer_info) {
  int err = 0;
  auto hnd = reinterpret_cast<SnapHandle *>(buffer_info->private_data);
  snapmapper_->Release(*hnd);

  AllocatedBufferInfo &alloc_buffer_info = buffer_info->alloc_buffer_info;

  alloc_buffer_info.fd = -1;
  alloc_buffer_info.stride = 0;
  alloc_buffer_info.size = 0;
  buffer_info->private_data = NULL;
  return err;
}

int HWCBufferAllocator::GetHeight(void *buf, uint32_t &height) {
  uint32_t tmp_height;
  auto err = STABLEMAPPER(mapper_).getMetadata(
      static_cast<buffer_handle_t>(buf),
      VENDOR_QTI_METADATA(SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS), &tmp_height,
      sizeof(tmp_height));
  if (err >= 0) {
    height = tmp_height;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetWidth(void *buf, uint32_t &width) {
  auto result =
      GetStandardMetadata<StandardMetadataType::STRIDE>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    width = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetUnalignedHeight(void *buf, uint32_t &height) {
  auto result =
      GetStandardMetadata<StandardMetadataType::HEIGHT>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    height = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetUnalignedWidth(void *buf, uint32_t &width) {
  auto result =
      GetStandardMetadata<StandardMetadataType::WIDTH>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    width = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetFd(void *buf, int &fd) {
  int tmp_fd;
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::FD), &tmp_fd,
                                               sizeof(tmp_fd));
  if (err >= 0) {
    fd = tmp_fd;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetAllocationSize(void *buf, uint32_t &alloc_size) {
  auto result = GetStandardMetadata<StandardMetadataType::ALLOCATION_SIZE>(
      mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    alloc_size = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetBufferId(void *buf, uint64_t &id) {
  auto result = GetStandardMetadata<StandardMetadataType::BUFFER_ID>(
      mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    id = static_cast<uint32_t>(*result);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetFormat(void *buf, int32_t &format) {
  int32_t ret_format;
  int err =
      GetVendorMetadata(mapper_, static_cast<buffer_handle_t>(buf),
                        SnapMetadataType::PIXEL_FORMAT_ALLOCATED, &ret_format, sizeof(ret_format));

  if (err == AIMAPPER_ERROR_NONE) {
    format = ret_format;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetPrivateFlags(void *buf, int32_t &flags) {
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

    bool secure = buffer_usage & vendor_qti_hardware_display_common_BufferUsage::PROTECTED;
    flags = (secure) ? (flags | qtigralloc::PRIV_FLAGS_SECURE_BUFFER) : flags;
    flags =
        (secure && (buffer_usage & vendor_qti_hardware_display_common_BufferUsage::CAMERA_OUTPUT))
            ? (flags | qtigralloc::PRIV_FLAGS_CAMERA_WRITE)
            : flags;
    flags =
        (buffer_usage & vendor_qti_hardware_display_common_BufferUsage::QTI_PRIVATE_SECURE_DISPLAY)
            ? (flags | qtigralloc::PRIV_FLAGS_SECURE_DISPLAY)
            : flags;

    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetCompressionType(void *buf, int64_t &compression_type) {
  auto result = GetStandardMetadata<StandardMetadataType::COMPRESSION>(
      mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    ExtendableType temp_compression_type = static_cast<ExtendableType>(*result);
    compression_type = static_cast<int64_t>(temp_compression_type.value);
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetSDMFormat(void *buf, LayerBufferFormat &sdm_format) {
  int32_t tmp_format, tmp_flags, err;
  int64_t tmp_compression_type;
  err = GetFormat(buf, tmp_format);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetPrivateFlags(buf, tmp_flags);
  if (err != kErrorNone)
    return kErrorUndefined;

  err = GetCompressionType(buf, tmp_compression_type);
  if (err != kErrorNone)
    return kErrorUndefined;

  GetSDMFormat(tmp_format, tmp_flags, tmp_compression_type);

  return kErrorNone;
}

int HWCBufferAllocator::GetBufferType(void *buf, uint32_t &buffer_type) {
  int32_t tmp_buffer_type;
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::BUFFER_TYPE),
                                               &tmp_buffer_type, sizeof(tmp_buffer_type));
  if (err >= 0) {
    buffer_type = tmp_buffer_type;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetBufferGeometry(void *buf, int32_t &slice_width, int32_t &slice_height) {
  auto result =
      GetStandardMetadata<StandardMetadataType::CROP>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    slice_width = result.value()[0].right;
    slice_height = result.value()[0].bottom;
    return kErrorNone;
  }
  return kErrorParameters;
}

int HWCBufferAllocator::GetCustomWidthAndHeight(void *handle, int *width, int *height) {
  void *hnd = handle;

  GetMetadataValue(hnd, SnapMetadataType::STRIDE, width, sizeof(*width));
  GetMetadataValue(hnd, SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS, height, sizeof(*height));

  auto err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
  }
  int ret;
  if (handle != nullptr) {
    if (snap_helper_->IsSnapAllocEnabled()) {
      ret = GetMetadataValue(static_cast<native_handle_t *>(handle),
                             SnapMetadataType::CUSTOM_DIMENSIONS_STRIDE, width, sizeof(*width));

      if (ret == 0) {
        ret = GetMetadataValue(static_cast<native_handle_t *>(handle),
                               SnapMetadataType::CUSTOM_DIMENSIONS_HEIGHT, height, sizeof(*height));
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

  return kErrorNone;
}

int HWCBufferAllocator::GetAlignedWidthAndHeight(int width, int height, int format,
                                                 uint32_t alloc_type, int *aligned_width,
                                                 int *aligned_height) {
  uint64_t usage = 0;
  unsigned int alignedw, alignedh;
  if (alloc_type & GRALLOC_USAGE_HW_FB) {
    usage |= static_cast<uint64_t>(ABufferUsage::COMPOSER_CLIENT_TARGET);
  }
  if (alloc_type & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    usage |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
  }
  *aligned_width = width;
  *aligned_height = height;

  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
  }
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
}

uint32_t HWCBufferAllocator::GetBufferSize(BufferInfo *buffer_info) {
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return 0;
  }

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

  if (snap_helper_->IsSnapAllocEnabled()) {
    gralloc::BufferDescriptor buf_desc;
    buf_desc.SetColorFormat(format);
    buf_desc.SetDimensions(width, height);
    buf_desc.SetUsage(alloc_flags);
    if (!mapper::GetFromBufferDescriptor(mapper::ConvertGrallocToAidlDescriptor(buf_desc),
                                         SnapMetadataType::ALLOCATION_SIZE,
                                         static_cast<void *>(&buffer_size), false)) {
      return buffer_size;
    }
  } else {
    uint32_t aligned_width = 0, aligned_height = 0;
    gralloc::BufferInfo info(static_cast<int>(width), static_cast<int>(height),
                             static_cast<int>(static_cast<APixelFormat>(format)), alloc_flags);
    if (gralloc::GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height) ==
        0) {
      return buffer_size;
    }
  }

  return 0;
}

int HWCBufferAllocator::SetBufferInfo(LayerBufferFormat format, int *target, uint64_t *flags) {
  switch (format) {
    case kFormatRGBA8888:
      *target = static_cast<int>(APixelFormat::RGBA_8888);
      break;
    case kFormatRGBX8888:
      *target = static_cast<int>(APixelFormat::RGBX_8888);
      break;
    case kFormatRGB888:
      *target = static_cast<int>(APixelFormat::RGB_888);
      break;
    case kFormatRGB565:
      *target = static_cast<int>(APixelFormat::RGB_565);
      break;
    case kFormatBGR565:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      break;
    case kFormatBGR888:
      *target = HAL_PIXEL_FORMAT_BGR_888;
      break;
    case kFormatBGRA8888:
    case kFormatARGB8888:
      *target = static_cast<int>(APixelFormat::BGRA_8888);
      break;
    case kFormatYCrCb420PlanarStride16:
      *target = static_cast<int>(APixelFormat::YV12);
      break;
    case kFormatYCrCb420SemiPlanar:
      *target = static_cast<int>(APixelFormat::YCRCB_420_SP);
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
      *target = static_cast<int>(APixelFormat::YCBCR_422_SP);
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
      *target = static_cast<int>(APixelFormat::RGBA_1010102);
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
      *target = static_cast<int>(APixelFormat::RGBA_8888);
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX8888Ubwc:
      *target = static_cast<int>(APixelFormat::RGBX_8888);
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatBGR565Ubwc:
      *target = HAL_PIXEL_FORMAT_BGR_565;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA1010102Ubwc:
      *target = static_cast<int>(APixelFormat::RGBA_1010102);
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBX1010102Ubwc:
      *target = HAL_PIXEL_FORMAT_RGBX_1010102;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatBlob:
      *target = static_cast<int>(APixelFormat::BLOB);
      break;
    case kFormatRGBA16161616F:
      *target = HAL_PIXEL_FORMAT_RGBA_FP16;
      break;
    case kFormatRGBA16161616FUbwc:
      *target = HAL_PIXEL_FORMAT_RGBA_FP16;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA8888UbwcLossy8To5:
      *target = static_cast<int>(APixelFormat::RGBA_8888);
      *flags |= vendor_qti_hardware_display_common_BufferUsage::QTI_ALLOC_UBWC_L_8_TO_5;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatRGBA8888UbwcLossy2To1:
      *target = static_cast<int>(APixelFormat::RGBA_8888);
      *flags |= vendor_qti_hardware_display_common_BufferUsage::QTI_ALLOC_UBWC_L_2_TO_1;
      *flags |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
      break;
    case kFormatYCbCr422P210:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_P210;
      break;
    case kFormatYCbCr422P210Ubwc:
      *target = HAL_PIXEL_FORMAT_YCbCr_422_P210_UBWC;
      *flags |= vendor_qti_hardware_display_common_BufferUsage::QTI_ALLOC_UBWC;
      break;
    default:
      DLOGW("Unsupported format = 0x%x", format);
      return -EINVAL;
  }
  return 0;
}

int HWCBufferAllocator::GetAllocatedBufferInfo(const BufferConfig &buffer_config,
                                               AllocatedBufferInfo *allocated_buffer_info) {
  BufferInfo buffer_info = {buffer_config, *allocated_buffer_info};
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
    return -EINVAL;
  }

  uint32_t buffer_size = 0;
  int aligned_width = 0, aligned_height = 0;
  int ret =
      GetAlignedWidthAndHeight(width, height, format, alloc_flags, &aligned_width, &aligned_height);
  buffer_size = GetBufferSize(&buffer_info);
  if (ret < 0 || buffer_size == 0) {
    return -EINVAL;
  }
  allocated_buffer_info->stride = UINT32(aligned_width);
  allocated_buffer_info->aligned_width = UINT32(aligned_width);
  allocated_buffer_info->aligned_height = UINT32(aligned_height);
  allocated_buffer_info->size = UINT32(buffer_size);

  return 0;
}

int HWCBufferAllocator::GetBufferLayout(const AllocatedBufferInfo &buf_info, uint32_t stride[4],
                                        uint32_t offset[4], uint32_t *num_planes) {
  int err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
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
  if (snap_helper_->IsSnapAllocEnabled()) {
    // TODO: reduce code duplication here
    BufferDescriptorInfo info_aidl{
        .width = static_cast<int32_t>(buf_info.aligned_width),
        .height = static_cast<int32_t>(buf_info.aligned_height),
        .layerCount = 1,
        .format = static_cast<GrallocPixelFormat>(format),
        .usage = static_cast<GrallocBufferUsage>(buf_info.usage),
    };
    SnapBufferLayout buffer_layout = {};
    auto &plane_layout_info = buffer_layout.planes;
    if (!mapper::GetFromBufferDescriptor(info_aidl, SnapMetadataType::PLANE_LAYOUTS, &buffer_layout,
                                         false)) {
      int type;
      plane_count = buffer_layout.plane_count;
      DLOGV("%s: Number of planes - %d gralloc format %d", __FUNCTION__, plane_count, format);
      for (int i = 0; i < plane_count; i++) {
        type = 0;
        for (int j = 0; j < plane_layout_info[i].component_count; j++) {
          type |= static_cast<int>(plane_layout_info[i].components[j].type);
        }
        DLOGV("%s: plane info: component - %d", __FUNCTION__, type);
        DLOGV("h_subsampling - %u, v_subsampling - %u, offset - %u, pixel_increment/step - %d",
              plane_layout_info[i].horizontal_subsampling,
              plane_layout_info[i].vertical_subsampling, plane_layout_info[i].offset_in_bytes,
              (plane_layout_info[i].sample_increment_bits / 8));
        DLOGV("stride_pixel - %d, stride_bytes - %d, scanlines - %d, size - %u",
              static_cast<unsigned int>(
                  static_cast<float>(plane_layout_info[i].horizontal_stride_in_bytes) /
                  (static_cast<float>(plane_layout_info[i].sample_increment_bits) / 8.0f)),
              plane_layout_info[i].horizontal_stride_in_bytes, plane_layout_info[i].scanlines,
              plane_layout_info[i].size_in_bytes);
      }
      // We are only returning buffer layout for progressive or single field formats.
      *num_planes = (plane_count > 3) ? 2 : plane_count;

      for (int i = 0; i < *num_planes; i++) {
        offset[i] = static_cast<uint32_t>(plane_layout_info[i].offset_in_bytes);
        stride[i] = static_cast<uint32_t>(plane_layout_info[i].horizontal_stride_in_bytes);
      }
    } else {
      return -EINVAL;
    }
  } else {
    unsigned int alignedw = 0, alignedh = 0;
    int custom_format = gralloc::GetImplDefinedFormat(buf_info.usage, format);
    err = gralloc::GetBufferSizeAndDimensions(info, &size, &alignedw, &alignedh);
    if (err) {
      return -EINVAL;
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
      return -EINVAL;
    }
    // We are only returning buffer layout for progressive or single field formats.
    *num_planes = (plane_count > 3) ? 2 : plane_count;

    for (int i = 0; i < *num_planes; i++) {
      offset[i] = static_cast<uint32_t>(plane_layout_info[i].offset);
      stride[i] = static_cast<uint32_t>(plane_layout_info[i].stride_bytes);
    }
    DLOGV("%s: Number of plane - %d, custom_format - %d", __FUNCTION__, plane_count, custom_format);
  }

  if (flags & qtigralloc::PRIV_FLAGS_UBWC_ALIGNED) {
    std::fill(offset, offset + 4, 0);
  }

  return kErrorNone;
}

int HWCBufferAllocator::MapBuffer(void *handle, shared_ptr<Fence> acquire_fence, void **base_ptr) {
  auto err = GetGrallocInstance();
  if (err != 0) {
    DLOGW("Could not get gralloc instance");
    return err;
  }

  Fence::ScopedRef scoped_ref;
  NATIVE_HANDLE_DECLARE_STORAGE(acquire_fence_storage, 1, 0);
  int acquire_fence_fd = 0;
  if (acquire_fence) {
    acquire_fence_fd = scoped_ref.Get(acquire_fence);
  }

  auto hnd = static_cast<buffer_handle_t>(handle);
  *base_ptr = NULL;
  ARect access_region = {.left = 0, .top = 0, .right = 0, .bottom = 0};
  auto error = STABLEMAPPER(mapper_).lock(hnd, (uint64_t)ABufferUsage::CPU_READ_OFTEN,
                                          access_region, acquire_fence_fd, base_ptr);

  if (!*base_ptr || error != AIMAPPER_ERROR_NONE) {
    DLOGW("*base_ptr is NULL! lock(%p, ...) failed: %d", hnd, error);
    return kErrorUndefined;
  }

  return kErrorNone;
}

int HWCBufferAllocator::UnmapBuffer(void *handle, int *release_fence) {
  int err = kErrorNone;
  *release_fence = -1;
  auto hnd = static_cast<buffer_handle_t>(handle);
  auto error = STABLEMAPPER(mapper_).unlock(hnd, release_fence);

  if (error != AIMAPPER_ERROR_NONE) {
    ALOGW("unlock failed with error %d", error);
    err = -EINVAL;
  }

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
    }
    if (metadata_set) {
      error = GetVendorMetadata(mapper_, static_cast<buffer_handle_t>(buf), type, dest, dest_size);
    }

    if (error != AIMAPPER_ERROR_NONE || !metadata_set) {
      err = -ENOTSUP;
    }
  }

  return err;
}

// Returns true when color primary is supported
bool GetColorPrimary(const int32_t &dataspace, QtiColorPrimaries *color_primary) {
  auto standard = dataspace & HAL_DATASPACE_STANDARD_MASK;
  bool supported_csc = true;
  switch (standard) {
    case HAL_DATASPACE_STANDARD_BT709:
      *color_primary = QtiColorPrimaries::QtiColorPrimaries_BT709_5;
      break;
    case HAL_DATASPACE_STANDARD_BT601_525:
    case HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
      *color_primary = QtiColorPrimaries::QtiColorPrimaries_BT601_6_525;
      break;
    case HAL_DATASPACE_STANDARD_BT601_625:
    case HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
      *color_primary = QtiColorPrimaries::QtiColorPrimaries_BT601_6_625;
      break;
    case HAL_DATASPACE_STANDARD_DCI_P3:
      *color_primary = QtiColorPrimaries::QtiColorPrimaries_DCIP3;
      break;
    case HAL_DATASPACE_STANDARD_BT2020:
      *color_primary = QtiColorPrimaries::QtiColorPrimaries_BT2020;
      break;
    default:
      DLOGW_IF(kTagClient, "Unsupported Standard Request = %d", standard);
      supported_csc = false;
  }
  return supported_csc;
}

bool GetTransfer(const int32_t &dataspace, QtiGammaTransfer *gamma_transfer) {
  auto transfer = dataspace & HAL_DATASPACE_TRANSFER_MASK;
  bool supported_transfer = true;
  switch (transfer) {
    case HAL_DATASPACE_TRANSFER_SRGB:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_sRGB;
      break;
    case HAL_DATASPACE_TRANSFER_SMPTE_170M:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_SMPTE_170M;
      break;
    case HAL_DATASPACE_TRANSFER_ST2084:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_SMPTE_ST2084;
      break;
    case HAL_DATASPACE_TRANSFER_HLG:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_HLG;
      break;
    case HAL_DATASPACE_TRANSFER_LINEAR:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_Linear;
      break;
    case HAL_DATASPACE_TRANSFER_GAMMA2_2:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_Gamma2_2;
      break;
    case HAL_DATASPACE_TRANSFER_GAMMA2_8:
      *gamma_transfer = QtiGammaTransfer::QtiTransfer_Gamma2_8;
      break;
    default:
      DLOGW_IF(kTagClient, "Unsupported Transfer Request = %d", transfer);
      supported_transfer = false;
  }
  return supported_transfer;
} 

bool GetRange(const int32_t &dataspace, QtiColorRange *color_range) {
  auto range = dataspace & HAL_DATASPACE_RANGE_MASK;
  switch (range) {
    case HAL_DATASPACE_RANGE_FULL:
      *color_range = QtiColorRange::QtiRange_Full;
      break;
    case HAL_DATASPACE_RANGE_LIMITED:
      *color_range = QtiColorRange::QtiRange_Limited;
      break;
    case HAL_DATASPACE_RANGE_EXTENDED:
      *color_range = QtiColorRange::QtiRange_Extended;
      break;
    default:
      DLOGW_IF(kTagClient, "Unsupported Range Request = %d", range);
      return false;
  }
  return true;
}

// Retrieve ColorMetaData from android_data_space_t (STANDARD|TRANSFER|RANGE)
bool HWCBufferAllocator::GetSDMColorSpace(const int int_dataspace, QtiDataspace *dataspace) {
  bool valid = false;
  valid = GetColorPrimary(int_dataspace, &(dataspace->colorPrimaries));
  if (valid) {
    valid = GetTransfer(int_dataspace, &(dataspace->transfer));
  }
  if (valid) {
    valid = GetRange(int_dataspace, &(dataspace->range));
  }

  return valid;
}

LayerBufferFormat HWCBufferAllocator::GetSDMFormat(const int32_t &source, const int32_t flags,
                                         const int64_t compression_type) {
  LayerBufferFormat format = kFormatInvalid;
  if (flags & SnapMetadataType::IS_UBWC) {
    switch (source) {
      case static_cast<int>(PixelFormat::RGBA_8888):
        if (compression_type == QTI_COMPRESSION_UBWC_LOSSY_2_TO_1) {
          format = kFormatRGBA8888UbwcLossy2To1;
        } else if (compression_type == QTI_COMPRESSION_UBWC_LOSSY_8_TO_5) {
          format = kFormatRGBA8888UbwcLossy8To5;
        } else {
          format = kFormatRGBA8888Ubwc;
        }
        break;
      case static_cast<int>(PixelFormat::RGBX_8888):
        format = kFormatRGBX8888Ubwc;
        break;
      case HAL_PIXEL_FORMAT_BGR_565:
        format = kFormatBGR565Ubwc;
        break;
      case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
      case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
      case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
      case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        format = kFormatYCbCr420SPVenusUbwc;
        break;
      case static_cast<int>(PixelFormat::RGBA_1010102):
        format = kFormatRGBA1010102Ubwc;
        break;
      case HAL_PIXEL_FORMAT_RGBX_1010102:
        format = kFormatRGBX1010102Ubwc;
        break;
      case HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC:
        format = kFormatYCbCr420TP10Ubwc;
        break;
      case HAL_PIXEL_FORMAT_YCbCr_420_P010_UBWC:
      case HAL_PIXEL_FORMAT_YCbCr_420_P010_VENUS:
      case HAL_PIXEL_FORMAT_YCbCr_420_P010:
        format = kFormatYCbCr420P010Ubwc;
        break;
      case HAL_PIXEL_FORMAT_RGBA_FP16:
        format = kFormatRGBA16161616FUbwc;
        break;
      case HAL_PIXEL_FORMAT_YCbCr_422_P210_UBWC:
        format = kFormatYCbCr422P210Ubwc;
        break;
      case HAL_PIXEL_FORMAT_YCbCr_422_P210:
        format = kFormatYCbCr422P210;
        break;
      default:
        DLOGW("Unsupported format type for UBWC: %d", source);
        return kFormatInvalid;
    }
    return format;
  }

  switch (source) {
    case static_cast<int>(PixelFormat::RGBA_8888):
      format = kFormatRGBA8888;
      break;
    case HAL_PIXEL_FORMAT_RGBA_5551:
      format = kFormatRGBA5551;
      break;
    case HAL_PIXEL_FORMAT_RGBA_4444:
      format = kFormatRGBA4444;
      break;
    case static_cast<int>(PixelFormat::BGRA_8888):
      format = kFormatBGRA8888;
      break;
    case static_cast<int>(PixelFormat::RGBX_8888):
      format = kFormatRGBX8888;
      break;
    case HAL_PIXEL_FORMAT_BGRX_8888:
      format = kFormatBGRX8888;
      break;
    case static_cast<int>(PixelFormat::RGB_888):
      format = kFormatRGB888;
      break;
    case HAL_PIXEL_FORMAT_BGR_888:
      format = kFormatBGR888;
      break;
    case static_cast<int>(PixelFormat::RGB_565):
      format = kFormatRGB565;
      break;
    case HAL_PIXEL_FORMAT_BGR_565:
      format = kFormatBGR565;
      break;
    case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
      format = kFormatYCbCr420SemiPlanarVenus;
      break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP_VENUS:
      format = kFormatYCrCb420SemiPlanarVenus;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
      format = kFormatYCbCr420SPVenusUbwc;
      break;
    case static_cast<int>(PixelFormat::YV12):
      format = kFormatYCrCb420PlanarStride16;
      break;
    case static_cast<int>(PixelFormat::YCRCB_420_SP):
      format = kFormatYCrCb420SemiPlanar;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
      format = kFormatYCbCr420SemiPlanar;
      break;
    case static_cast<int>(PixelFormat::YCBCR_422_SP):
      format = kFormatYCbCr422H2V1SemiPlanar;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      format = kFormatYCbCr422H2V1Packed;
      break;
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
      format = kFormatCbYCrY422H2V1Packed;
      break;
    case static_cast<int>(PixelFormat::RGBA_1010102):
      format = kFormatRGBA1010102;
      break;
    case HAL_PIXEL_FORMAT_ARGB_2101010:
      format = kFormatARGB2101010;
      break;
    case HAL_PIXEL_FORMAT_RGBX_1010102:
      format = kFormatRGBX1010102;
      break;
    case HAL_PIXEL_FORMAT_XRGB_2101010:
      format = kFormatXRGB2101010;
      break;
    case HAL_PIXEL_FORMAT_BGRA_1010102:
      format = kFormatBGRA1010102;
      break;
    case HAL_PIXEL_FORMAT_ABGR_2101010:
      format = kFormatABGR2101010;
      break;
    case HAL_PIXEL_FORMAT_BGRX_1010102:
      format = kFormatBGRX1010102;
      break;
    case HAL_PIXEL_FORMAT_XBGR_2101010:
      format = kFormatXBGR2101010;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_P010:
      format = kFormatYCbCr420P010;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC:
      format = kFormatYCbCr420TP10Ubwc;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_P010_UBWC:
      format = kFormatYCbCr420P010Ubwc;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_420_P010_VENUS:
      format = kFormatYCbCr420P010Venus;
      break;
    case static_cast<int>(PixelFormat::RGBA_FP16):
      format = kFormatRGBA16161616F;
      break;
    case static_cast<int>(APixelFormat::R_8):
      format = kFormatA8;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_P210:
      format = kFormatYCbCr422P210;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_P210_UBWC:
      format = kFormatYCbCr422P210Ubwc;
      break;
    default:
      DLOGW("Unsupported format type = %d", source);
      return kFormatInvalid;
  }
  return format;
}

DisplayError HWCBufferAllocator::ColorMetadataToDataspace(Dataspace ds, uint32_t *int_dataspace) {
  AIDL_Dataspace primaries, transfer, range = AIDL_Dataspace::UNKNOWN;

  switch (ds.colorPrimaries) {
    case QtiColorPrimaries::QtiColorPrimaries_BT709_5:
      primaries = AIDL_Dataspace::STANDARD_BT709;
      break;
    // TODO(user): verify this is equivalent
    case QtiColorPrimaries::QtiColorPrimaries_BT470_6M:
      primaries = AIDL_Dataspace::STANDARD_BT470M;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_BT601_6_625:
      primaries = AIDL_Dataspace::STANDARD_BT601_625;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_BT601_6_525:
      primaries = AIDL_Dataspace::STANDARD_BT601_525;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_GenericFilm:
      primaries = AIDL_Dataspace::STANDARD_FILM;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_BT2020:
      primaries = AIDL_Dataspace::STANDARD_BT2020;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_AdobeRGB:
      primaries = AIDL_Dataspace::STANDARD_ADOBE_RGB;
      break;
    case QtiColorPrimaries::QtiColorPrimaries_DCIP3:
      primaries = AIDL_Dataspace::STANDARD_DCI_P3;
      break;
    default:
      return kErrorNotSupported;

       /*QtiColorPrimaries::QtiColorPrimaries_SMPTE_240M;
       QtiColorPrimaries::QtiColorPrimaries_SMPTE_ST428;
       QtiColorPrimaries::QtiColorPrimaries_EBU3213;*/

  }

  switch (ds.transfer) {
    case QtiGammaTransfer::QtiTransfer_sRGB:
      transfer = AIDL_Dataspace::TRANSFER_SRGB;
      break;
    case QtiGammaTransfer::QtiTransfer_Gamma2_2:
      transfer = AIDL_Dataspace::TRANSFER_GAMMA2_2;
      break;
    case QtiGammaTransfer::QtiTransfer_Gamma2_8:
      transfer = AIDL_Dataspace::TRANSFER_GAMMA2_8;
      break;
    case QtiGammaTransfer::QtiTransfer_SMPTE_170M:
      transfer = AIDL_Dataspace::TRANSFER_SMPTE_170M;
      break;
    case QtiGammaTransfer::QtiTransfer_Linear:
      transfer = AIDL_Dataspace::TRANSFER_LINEAR;
      break;
    case QtiGammaTransfer::QtiTransfer_SMPTE_ST2084:
      transfer = AIDL_Dataspace::TRANSFER_ST2084;
      break;
    case QtiGammaTransfer::QtiTransfer_HLG:
      transfer = AIDL_Dataspace::TRANSFER_HLG;
      break;
    default:
      return kErrorNotSupported;

      /*QtiGammaTransfer::QtiTransfer_SMPTE_240M
      QtiGammaTransfer::QtiTransfer_Log
      QtiGammaTransfer::QtiTransfer_Log_Sqrt
      QtiGammaTransfer::QtiTransfer_XvYCC
      QtiGammaTransfer::QtiTransfer_BT1361
      QtiGammaTransfer::QtiTransfer_sYCC
      QtiGammaTransfer::QtiTransfer_BT2020_2_1
      QtiGammaTransfer::QtiTransfer_BT2020_2_2
      QtiGammaTransfer::QtiTransfer_ST_428*/

  }

  switch (ds.range) {
    case QtiColorRange::QtiRange_Full:
      range = AIDL_Dataspace::RANGE_FULL;
      break;
    case QtiColorRange::QtiRange_Limited:
      range = AIDL_Dataspace::RANGE_LIMITED;
      break;
    case QtiColorRange::QtiRange_Extended:
      range = AIDL_Dataspace::RANGE_EXTENDED;
      break;
    default:
      return kErrorNotSupported;
  }

  *int_dataspace = ((uint32_t)primaries | (uint32_t)transfer | (uint32_t)range);
  return kErrorNone;
}

int32_t HWCBufferAllocator::TranslateFromLegacyDataspace(const int32_t &legacy_ds) {
  int32_t dataspace = legacy_ds;

  if (dataspace & 0xffff) {
    switch (dataspace & 0xffff) {
      case HAL_DATASPACE_SRGB:
        dataspace = HAL_DATASPACE_V0_SRGB;
        break;
      case HAL_DATASPACE_JFIF:
        dataspace = HAL_DATASPACE_V0_JFIF;
        break;
      case HAL_DATASPACE_SRGB_LINEAR:
        dataspace = HAL_DATASPACE_V0_SRGB_LINEAR;
        break;
      case HAL_DATASPACE_BT601_625:
        dataspace = HAL_DATASPACE_V0_BT601_625;
        break;
      case HAL_DATASPACE_BT601_525:
        dataspace = HAL_DATASPACE_V0_BT601_525;
        break;
      case HAL_DATASPACE_BT709:
        dataspace = HAL_DATASPACE_V0_BT709;
        break;
      default:
        // unknown legacy dataspace
        DLOGW_IF(kTagClient, "Unsupported dataspace type %d", dataspace);
    }
  }

  if (dataspace == 0) {
    dataspace = HAL_DATASPACE_V0_SRGB;
  }

  return dataspace;
}

}  // namespace sdm
