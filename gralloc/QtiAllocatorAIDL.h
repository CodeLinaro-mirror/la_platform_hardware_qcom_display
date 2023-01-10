/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QTIALLOCATORAIDL_H__
#define __QTIALLOCATORAIDL_H__

#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <binder/Status.h>

#include <vector>

#include "gr_buf_mgr.h"
#include "gr_utils.h"

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace impl {

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

class QtiAllocatorAIDL : public BnAllocator {
 public:
  QtiAllocatorAIDL();

  ndk::ScopedAStatus allocate(const std::vector<uint8_t>& descriptor, int32_t count,
                                 AllocationResult* result) override;

 private:
  BufferManager *buf_mgr_ = nullptr;
  bool enable_logs_;
};

}  // namespace impl
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl

#endif  // __QTIALLOCATORAIDL_H__
