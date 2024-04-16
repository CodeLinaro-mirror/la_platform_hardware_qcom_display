/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "QtiAllocatorAIDL.h"

#include <cutils/properties.h>
#include <log/log.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <vendor/qti/hardware/display/mapper/4.0/IQtiMapper.h>
#include <display_properties.h>

#include <vector>

#include "QtiMapper4.h"
#include "gr_utils.h"

using gralloc::Error;

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace impl {

static void GetProperties(gralloc::GrallocProperties *props) {
  props->use_system_heap_for_sensors = property_get_bool(USE_SYSTEM_HEAP_FOR_SENSORS, 1);

  props->ubwc_disable = property_get_bool(DISABLE_UBWC_PROP, 0);

  props->ahardware_buffer_disable = property_get_bool(DISABLE_AHARDWAREBUFFER_PROP, 0);
}

static inline ndk::ScopedAStatus ToBinderStatus(Error error) {
  AllocationError ret;

  switch (error) {
    case Error::BAD_DESCRIPTOR:
      ret = AllocationError::BAD_DESCRIPTOR;
      break;
    case Error::NO_RESOURCES:
      ret = AllocationError::NO_RESOURCES;
      break;
    case Error::UNSUPPORTED:
      ret = AllocationError::UNSUPPORTED;
      break;
    default:
      return ndk::ScopedAStatus::ok();
  }

  return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(ret));
}

QtiAllocatorAIDL::QtiAllocatorAIDL() {
  gralloc::GrallocProperties properties;
  GetProperties(&properties);
  buf_mgr_ = BufferManager::GetInstance();
  buf_mgr_->SetGrallocDebugProperties(properties);
  enable_logs_ = property_get_bool("vendor.gralloc.enable_logs", 0);
}

ndk::ScopedAStatus QtiAllocatorAIDL::allocate(const std::vector<uint8_t>& descriptor, int32_t count,
                                                 AllocationResult* result) {
  ALOGD_IF(enable_logs_, "Allocating buffers count: %d", count);
  gralloc::BufferDescriptor desc;

  auto err = ::vendor::qti::hardware::display::mapper::V4_0::implementation::QtiMapper::Decode(
      descriptor, &desc);
  if (err != Error::NONE) {
    ALOGE("Failed to allocate. Can't decode buffer descriptor: %d", err);
    return ToBinderStatus(err);
  }

  std::vector<buffer_handle_t> buffers;
  buffers.reserve(count);

  for (uint32_t i = 0; i < count; i++) {
    buffer_handle_t buffer;
    err = buf_mgr_->AllocateBuffer(desc, &buffer);
    if (err != Error::NONE) {
      return ToBinderStatus(err);
    }
    buffers.emplace_back(buffer);
  }

  if (buffers.size() > 0) {
    result->stride = static_cast<uint32_t>(QTI_HANDLE_CONST(buffers[0])->width);
  }

  result->buffers.resize(count);
  for (int32_t i = 0; i < count; i++) {
    auto buffer = buffers[i];
    result->buffers[i] = ::android::dupToAidl(buffer);
    buf_mgr_->ReleaseBuffer(QTI_HANDLE_CONST(buffer));
  }

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus QtiAllocatorAIDL::AllocateBuffer(gralloc::BufferDescriptor desc, int32_t count,
                                                    AllocationResult *result) {
  std::vector<buffer_handle_t> buffers;
  auto err = Error::UNSUPPORTED;

  buffers.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    buffer_handle_t buffer;
    err = buf_mgr_->AllocateBuffer(desc, &buffer);
    if (err != Error::NONE) {
      return ToBinderStatus(err);
    }
    buffers.emplace_back(buffer);
  }

  if (buffers.size() > 0) {
    result->stride = static_cast<uint32_t>(QTI_HANDLE_CONST(buffers[0])->width);
  }

  result->buffers.resize(count);
  for (int32_t i = 0; i < count; i++) {
    auto buffer = buffers[i];
    result->buffers[i] = ::android::dupToAidl(buffer);
  }

  // TODO: enable allocation data dumping
  /*if (enable_allocation_data_dumping_) {
     dumpAllocationData(buffers, result, desc, count);
  }*/

  for (int32_t i = 0; i < count; i++) {
    buffer_handle_t buffer = buffers[i];
    buf_mgr_->ReleaseBuffer(QTI_HANDLE_CONST(buffer));
  }
  return ndk::ScopedAStatus::ok();
}

static gralloc::BufferDescriptor convertAidlToGrallocDescriptor(const BufferDescriptorInfo &info) {
  gralloc::BufferDescriptor desc;

  desc.SetName(std::string(reinterpret_cast<const char *>(info.name.data())));
  desc.SetDimensions(static_cast<int>(info.width), static_cast<int>(info.height));
  desc.SetLayerCount(static_cast<uint32_t>(info.layerCount));
  desc.SetColorFormat(static_cast<int>(info.format));
  desc.SetUsage(static_cast<uint64_t>(info.usage));
  desc.SetReservedSize(static_cast<uint64_t>(info.reservedSize));
  return desc;
}

ndk::ScopedAStatus QtiAllocatorAIDL::allocate2(const BufferDescriptorInfo &in_descriptor,
                                               int32_t in_count, AllocationResult *_aidl_return) {
  ALOGD_IF(enable_logs_, "Allocating buffers count: %d", in_count);
  if (!in_descriptor.additionalOptions.empty()) {
    return ToBinderStatus(Error::UNSUPPORTED);
  }

  gralloc::BufferDescriptor desc = convertAidlToGrallocDescriptor(in_descriptor);
  return AllocateBuffer(desc, in_count, _aidl_return);
}
ndk::ScopedAStatus QtiAllocatorAIDL::getIMapperLibrarySuffix(std::string *_aidl_return) {
  *_aidl_return = "qti";
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus QtiAllocatorAIDL::isSupported(const BufferDescriptorInfo &in_descriptor,
                                                 bool *_aidl_return) {
  if (!in_descriptor.additionalOptions.empty()) {
    *_aidl_return = false;
    return ndk::ScopedAStatus::ok();
  }

  gralloc::BufferDescriptor desc = convertAidlToGrallocDescriptor(in_descriptor);
  buffer_handle_t buffer;
  if (buf_mgr_->AllocateBuffer(desc, &buffer, 0, true) != Error::NONE) {
    *_aidl_return = false;
  }
  *_aidl_return = true;
  return ndk::ScopedAStatus::ok();
}

}  // namespace impl
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
