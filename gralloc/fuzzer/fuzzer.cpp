/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <fuzzbinder/libbinder_ndk_driver.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <log/log.h>

#include "QtiAllocatorAIDL.h"

using aidl::android::hardware::graphics::allocator::impl::QtiAllocatorAIDL;

std::shared_ptr<QtiAllocatorAIDL> service;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  service = ndk::SharedRefBase::make<QtiAllocatorAIDL>();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  android::fuzzService(service->asBinder().get(), FuzzedDataProvider(data, size));

  return 0;
}