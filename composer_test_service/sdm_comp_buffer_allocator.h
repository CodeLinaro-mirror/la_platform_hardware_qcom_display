/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
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
 *
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __SDM_COMP_BUFFER_ALLOCATOR_H__
#define __SDM_COMP_BUFFER_ALLOCATOR_H__

#include <fcntl.h>
#include <sys/mman.h>
#include <QtiGrallocMetadata.h>

#include <android/hardware/graphics/common/1.2/types.h>
#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/IAllocator.h>
#include <android/binder_manager.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <QtiGrallocPriv.h>
#include <core/buffer_allocator.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <vndksupport/linker.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>

using aidl::android::hardware::graphics::allocator::AllocationResult;
using aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using aidl::android::hardware::graphics::allocator::IAllocator;
using aidl::android::hardware::graphics::common::ExtendableType;

namespace sdm {

template <class Type>
inline Type ALIGN(Type x, Type align) {
  return (x + align - 1) & ~(align - 1);
}

class SDMCompBufferAllocator : public BufferAllocator {
 public:
  int AllocateBuffer(BufferInfo *buffer_info);
  int FreeBuffer(BufferInfo *buffer_info);
  uint32_t GetBufferSize(BufferInfo *buffer_info);

  int GetCustomWidthAndHeight(void *handle, int *width, int *height);
  int GetAlignedWidthAndHeight(int width, int height, int format, uint32_t alloc_type,
                               int *aligned_width, int *aligned_height);
  int GetAllocatedBufferInfo(const BufferConfig &buffer_config,
                                      AllocatedBufferInfo *allocated_buffer_info);
  int GetBufferLayout(const AllocatedBufferInfo &buf_info, uint32_t stride[4],
                               uint32_t offset[4], uint32_t *num_planes);
  int SetBufferInfo(LayerBufferFormat format, int *target, uint64_t *flags);
  int MapBuffer(void *handle, shared_ptr<Fence> acquire_fence, void **base_ptr);
  int UnmapBuffer(void *handle, int *release_fence);
  int GetHeight(void *buf, uint32_t &height);
  int GetWidth(void *buf, uint32_t &width);
  int GetUnalignedHeight(void *buf, uint32_t &height);
  int GetUnalignedWidth(void *buf, uint32_t &width);
  int GetFd(void *buf, int &fd);
  int GetAllocationSize(void *buf, uint32_t &alloc_size);
  int GetBufferId(void *buf, uint64_t &id);
  int GetFormat(void *buf, int32_t &format);
  int GetPrivateFlags(void *buf, int32_t &flags);
  int GetSDMFormat(void *buf, LayerBufferFormat &sdm_format);
  int GetBufferType(void *buf, uint32_t &buffer_type);
  int GetBufferGeometry(void *buf, int32_t &slice_width, int32_t &slice_height);
  int GetCustomContentMetadata(void *buf, CustomContentMetadata *dest);
  int GetCompressionType(void *buf, int64_t &compression_type);

  // callbacks from sdmclient
  bool GetSDMColorSpace(const int int_dataspace, QtiDataspace *dataspace){return 0;}
  LayerBufferFormat GetSDMFormat(const int32_t &source, const int32_t flags,
                                 const int64_t compression_type) {
    return kFormatInvalid;
  }
  DisplayError ColorMetadataToDataspace(Dataspace ds, uint32_t *int_dataspace) {return kErrorNone;}
  int32_t TranslateFromLegacyDataspace(const int32_t &legacy_ds) {return 0;}
  int GetMetadataValue(void *buf, vendor_qti_hardware_display_common_MetadataType type, void *dest,
                       size_t dest_size);

 private:
  int GetGrallocInstance();
  void SetBufferAccessControlInfo(std::bitset<kBufferPermMax> perm, BufferPermission *buf_perm);
  LayerBufferFormat GetFormatSDM(const int32_t &source, const int flags,
                                 const int64_t compression_type);
  std::shared_ptr<IAllocator> allocator_ = nullptr;
  AIMapper *mapper_ = nullptr;
};

}  // namespace sdm
#endif  // __SDM_COMP_BUFFER_ALLOCATOR_H__
