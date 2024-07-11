/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <QtiGralloc.h>

#include <gralloctypes/Gralloc4.h>
#include <core/buffer_allocator.h>
#include <utils/constants.h>
#include <utils/debug.h>
#include <errno.h>
#include <QtiGrallocDefs.h>

#include "gr_utils.h"
#include "hwc_buffer_allocator.h"
#include "hwc_debugger.h"
#include "hwc_layers.h"
#include "gr_snap_helper.h"
#include "mapper_utils.h"

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <android/rect.h>

#define __CLASS__ "HWCBufferAllocator"

using mapper::GetMapperInstance;
using mapper::GetMetadataState;
using mapper::GetStandardMetadata;
using mapper::GetVendorMetadata;
using mapper::IsSettable;

using APixelFormat = aidl::android::hardware::graphics::common::PixelFormat;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::common::StandardMetadataType;
using ABufferUsage = aidl::android::hardware::graphics::common::BufferUsage;

namespace sdm {

DisplayError HWCBufferAllocator::GetGrallocInstance() {
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

  mapper_ = GetMapperInstance();
  if (mapper_ == nullptr) {
    DLOGE("Unable to get mapper");
    return kErrorCriticalResource;
  }

  if (snap_helper_ == nullptr) {
    snap_helper_ = gralloc::GrallocSnapHelper::GetInstance();
    if (snap_helper_ == nullptr) {
      DLOGW("Unable to get snap helper");
    }
  }

  return kErrorNone;
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
    alloc_flags |= static_cast<uint64_t>(ABufferUsage::PROTECTED);
  }

  if (buffer_config.secure_camera) {
    alloc_flags |= static_cast<uint64_t>(ABufferUsage::CAMERA_OUTPUT);
  }

  if (!buffer_config.cache) {
    // Allocate uncached buffers
    alloc_flags |= GRALLOC_USAGE_PRIVATE_UNCACHED;
  }

  if (buffer_config.gfx_client) {
    alloc_flags |= static_cast<uint64_t>(ABufferUsage::GPU_TEXTURE);
  }

  alloc_flags |= static_cast<uint64_t>(ABufferUsage::COMPOSER_OVERLAY);


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

  private_handle_t *hnd = (private_handle_t *)raw_handle;
  alloc_buffer_info->fd = hnd->fd;
  alloc_buffer_info->stride = UINT32(hnd->width);
  alloc_buffer_info->aligned_width = UINT32(hnd->width);
  alloc_buffer_info->aligned_height = UINT32(hnd->height);
  alloc_buffer_info->size = hnd->size;
  alloc_buffer_info->id = hnd->id;
  alloc_buffer_info->format = HWCLayer::GetSDMFormat(hnd->format, hnd->flags);

  buffer_info->private_data = reinterpret_cast<void *>(hnd);
  return kErrorNone;
}

DisplayError HWCBufferAllocator::FreeBuffer(BufferInfo *buffer_info) {
  DisplayError err = kErrorNone;
  auto hnd = reinterpret_cast<buffer_handle_t>(buffer_info->private_data);
  if (mapper_ != nullptr) {
    STABLEMAPPER(mapper_).freeBuffer(hnd);
  }
  AllocatedBufferInfo &alloc_buffer_info = buffer_info->alloc_buffer_info;

  alloc_buffer_info.fd = -1;
  alloc_buffer_info.stride = 0;
  alloc_buffer_info.size = 0;
  buffer_info->private_data = NULL;
  return err;
}

int HWCBufferAllocator::GetBufferGeometry(void *buf, int32_t &slice_width, int32_t &slice_height) {
  auto result =
      GetStandardMetadata<StandardMetadataType::CROP>(mapper_, static_cast<buffer_handle_t>(buf));

  if (result.has_value()) {
    slice_width = result.value()[0].right - result.value()[0].left;
    slice_height = result.value()[0].bottom - result.value()[0].top;
    return kErrorNone;
  }
  return kErrorParameters;
}

void HWCBufferAllocator::GetCustomWidthAndHeight(const private_handle_t *handle, int *width,
                                                 int *height) {
  void *hnd = const_cast<private_handle_t *>(handle);

  GetMetadataValue(hnd, SnapMetadataType::STRIDE, width, sizeof(*width));
  GetMetadataValue(hnd, SnapMetadataType::ALIGNED_HEIGHT_IN_PIXELS, height, sizeof(*height));

  auto err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return;
  }
  int ret;
  if (handle != nullptr) {
    if (snap_helper_->IsSnapAllocEnabled()) {
      ret = snap_helper_->GetMetadata(const_cast<private_handle_t *>(handle),
                                      SnapMetadataType::CUSTOM_DIMENSIONS_STRIDE, width,
                                      false);
      if (ret == 0) {
        ret = snap_helper_->GetMetadata(const_cast<private_handle_t *>(handle),
                                        SnapMetadataType::CUSTOM_DIMENSIONS_HEIGHT, height, false);
      }
    } else {
      ret = gralloc::GetCustomDimensions(static_cast<private_handle_t *>(hnd), width, height);
    }
    if (ret) {
      ALOGW("%s: Error obtaining custom dimensions. "
            "stride: %d, height: %d",
            __FUNCTION__, *width, *height);
      return;
    }
  }

  return;
}

void HWCBufferAllocator::GetAlignedWidthAndHeight(int width, int height, int format,
                                                  uint32_t alloc_type, int *aligned_width,
                                                  int *aligned_height) {
  uint64_t usage = 0;
  unsigned int alignedw = 0, alignedh = 0;
  int err = 0;
  if (alloc_type & GRALLOC_USAGE_HW_FB) {
    usage |= static_cast<uint64_t>(ABufferUsage::COMPOSER_CLIENT_TARGET);
  }
  if (alloc_type & GRALLOC_USAGE_PRIVATE_ALLOC_UBWC) {
    usage |= GRALLOC_USAGE_PRIVATE_ALLOC_UBWC;
  }
  uint32_t aligned_h = UINT(height);
  uint32_t aligned_w = UINT(width);

  if (snap_helper_ == nullptr) {
    snap_helper_ = gralloc::GrallocSnapHelper::GetInstance();
    if (snap_helper_ == nullptr) {
      DLOGW("Unable to get snap helper");
    }
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
    gralloc::GetAlignedWidthAndHeight(info, &aligned_w, &aligned_h);
    *aligned_width = INT(aligned_w);
    *aligned_height = INT(aligned_h);
  }
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
    uint32_t aligned_width = 0, aligned_height = 0, buffer_size = 0;
    gralloc::BufferInfo info(static_cast<int>(width), static_cast<int>(height),
                             static_cast<int>(static_cast<APixelFormat>(format)), alloc_flags);
    int ret = GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height);
    if (ret < 0) {
      return 0;
    }
    return buffer_size;
  }
  return 0;
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
      DLOGW("Unsupported format = 0x%x", format);
      return -EINVAL;
  }
  return 0;
}

DisplayError HWCBufferAllocator::GetAllocatedBufferInfo(
    const BufferConfig &buffer_config, AllocatedBufferInfo *allocated_buffer_info) {
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

  uint32_t aligned_width = 0, aligned_height = 0, buffer_size = 0;
  gralloc::BufferInfo info(width, height, format, alloc_flags);
  int ret = GetBufferSizeAndDimensions(info, &buffer_size, &aligned_width, &aligned_height);
  if (ret < 0) {
    return kErrorParameters;
  }
  allocated_buffer_info->stride = UINT32(aligned_width);
  allocated_buffer_info->aligned_width = UINT32(aligned_width);
  allocated_buffer_info->aligned_height = UINT32(aligned_height);
  allocated_buffer_info->size = UINT32(buffer_size);

  return kErrorNone;
}

DisplayError HWCBufferAllocator::GetBufferLayout(const AllocatedBufferInfo &buf_info,
                                                 uint32_t stride[4], uint32_t offset[4],
                                                 uint32_t *num_planes) {
  DisplayError err;
  err = GetGrallocInstance();
  if (err != 0) {
    DLOGE("Failed to retrieve gralloc instance");
    return err;
  }
  int format = HAL_PIXEL_FORMAT_RGBA_8888;

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
        buf_info.aligned_width, buf_info.aligned_height, flags, format);

  gralloc::BufferInfo info(buf_info.aligned_width, buf_info.aligned_height, format, flags);
  if (snap_helper_->IsSnapAllocEnabled()) {
    // TODO: reduce code duplication here
    BufferDescriptorInfo info_aidl{
        .width = static_cast<int32_t>(buf_info.aligned_width),
        .height = static_cast<int32_t>(buf_info.aligned_height),
        .layerCount = 1,
        .format = static_cast<GrallocPixelFormat>(format),
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
      return kErrorParameters;
    }
  } else {
    unsigned int alignedw = 0, alignedh = 0;
    int custom_format = gralloc::GetImplDefinedFormat(flags, format);
    int error = gralloc::GetBufferSizeAndDimensions(info, &size, &alignedw, &alignedh);
    if (error) {
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

DisplayError HWCBufferAllocator::MapBuffer(const private_handle_t *handle,
                                           shared_ptr<Fence> acquire_fence) {
  auto err = GetGrallocInstance();
  if (err != kErrorNone) {
    return err;
  }

  Fence::ScopedRef scoped_ref;
  NATIVE_HANDLE_DECLARE_STORAGE(acquire_fence_storage, 1, 0);
  int acquire_fence_fd = 0;
  if (acquire_fence) {
    acquire_fence_fd = scoped_ref.Get(acquire_fence);
  }

  auto hnd = static_cast<buffer_handle_t>(const_cast<private_handle_t *>(handle));
  void **base_ptr = NULL;
  ARect access_region = {.left = 0, .top = 0, .right = 0, .bottom = 0};
  auto error = STABLEMAPPER(mapper_).lock(hnd, (uint64_t)ABufferUsage::CPU_READ_OFTEN,
               access_region, acquire_fence_fd, base_ptr);

  if (!*base_ptr || error != AIMAPPER_ERROR_NONE) {
    DLOGW("*base_ptr is NULL! lock(%p, ...) failed: %d", hnd, error);
    return kErrorUndefined;
  }
  return kErrorNone;
}

DisplayError HWCBufferAllocator::UnmapBuffer(const private_handle_t *handle, int *release_fence) {
  DisplayError err = kErrorNone;
  *release_fence = -1;
  auto hnd = static_cast<buffer_handle_t>(const_cast<private_handle_t *>(handle));
  auto error = STABLEMAPPER(mapper_).unlock(hnd, release_fence);

  if (error != AIMAPPER_ERROR_NONE) {
    ALOGW("unlock failed with error %d", error);
    err = kErrorUndefined;
  }
  return err;
}

int HWCBufferAllocator::GetPrivateFlags(void *buf, int32_t &flags) {
  int32_t is_ubwc = 0, is_tile_rendered = 0, is_cached = 0;
  auto err = STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                               VENDOR_QTI_METADATA(SnapMetadataType::IS_UBWC),
                                               &is_ubwc, sizeof(is_ubwc));
  err |= STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                           VENDOR_QTI_METADATA(SnapMetadataType::IS_TILE_RENDERED),
                                           &is_tile_rendered, sizeof(is_tile_rendered));
  err |= STABLEMAPPER(mapper_).getMetadata(static_cast<buffer_handle_t>(buf),
                                           VENDOR_QTI_METADATA(SnapMetadataType::IS_CACHED),
                                           &is_cached, sizeof(is_cached));
  if (err >= 0) {
    flags = is_ubwc ? (flags | qtigralloc::PRIV_FLAGS_UBWC_ALIGNED) : flags;
    flags = is_tile_rendered ? (flags | qtigralloc::PRIV_FLAGS_TILE_RENDERED) : flags;
    flags = is_cached ? (flags | qtigralloc::PRIV_FLAGS_CACHED) : flags;
    return kErrorNone;
  }
  return kErrorParameters;
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

}  // namespace sdm
