/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QTIALLOCATORAIDL_H__
#define __QTIALLOCATORAIDL_H__

#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <binder/Status.h>

#include <vector>
#include <json/json.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <mutex>

#include "gr_buf_mgr.h"
#include "gr_utils.h"
#include "gr_snap_helper.h"

#include "display_properties.h"

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace impl {

using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hidl::base::V1_0::DebugInfo;
using ::android::hidl::base::V1_0::IBase;
using gralloc::BufferManager;

using IMapper_v4 = ::android::hardware::graphics::mapper::V4_0::IMapper;
using AidlPlaneLayout = aidl::android::hardware::graphics::common::PlaneLayout;
using Error_v4 = ::android::hardware::graphics::mapper::V4_0::Error;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;

class QtiAllocatorAIDL : public BnAllocator {
 public:
  QtiAllocatorAIDL();

  ndk::ScopedAStatus allocate(const std::vector<uint8_t> &descriptor, int32_t count,
                              AllocationResult *result) override;
  ndk::ScopedAStatus allocate2(const BufferDescriptorInfo &in_descriptor, int32_t in_count,
                               AllocationResult *_aidl_return) override;
  ndk::ScopedAStatus getIMapperLibrarySuffix(std::string *_aidl_return) override;
  ndk::ScopedAStatus isSupported(const BufferDescriptorInfo &in_descriptor,
                                 bool *_aidl_return) override;

 protected:
  ndk::SpAIBinder createBinder() override;

 private:
  ndk::ScopedAStatus AllocateBuffer(gralloc::BufferDescriptor desc, int32_t count,
                                    AllocationResult *result);
  BufferManager *buf_mgr_ = nullptr;
  gralloc::GrallocSnapHelper *snap_helper_ = nullptr;
  bool enable_logs_;
  bool enable_allocation_data_dumping_;
  std::string json_file_name_;
  std::mutex json_dump_lock_;
  bool is_json_first_entry_ = true;
  int dumpAllocationData(std::vector<buffer_handle_t> buffers, AllocationResult *result,
                         gralloc::BufferDescriptor desc, int32_t count);
};

}  // namespace impl
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl

#endif  // __QTIALLOCATORAIDL_H__
