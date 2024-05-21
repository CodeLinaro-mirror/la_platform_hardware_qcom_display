/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <fuzzbinder/libbinder_ndk_driver.h>
#include <fuzzer/FuzzedDataProvider.h>

#include <android-base/logging.h>
#include <android/binder_interface_utils.h>

using android::fuzzService;
using ndk::SharedRefBase;
#include "DisplayConfigAIDL.h"

using namespace ::aidl::vendor::qti::hardware::display::config;

using ::ndk::ScopedAStatus;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  auto binder = ::ndk::SharedRefBase::make<DisplayConfigAIDL>();

  fuzzService(binder->asBinder().get(), FuzzedDataProvider(data, size));

  return 0;
}
