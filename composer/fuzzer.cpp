/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
std::shared_ptr<DisplayConfigAIDL> serviceDisplayConfigAIDL;

using ::ndk::ScopedAStatus;

#ifdef COMPOSER3_V5
#include "AmbientDataCaptureAIDL.h"
using namespace ::aidl::vendor::qti::hardware::qacs::ambientdatacapture;
std::shared_ptr<AmbientDataCaptureAIDL> serviceAmbientDataCaptureAIDL;
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  serviceDisplayConfigAIDL = ndk::SharedRefBase::make<DisplayConfigAIDL>();

#ifdef COMPOSER3_V5
  serviceAmbientDataCaptureAIDL = ndk::SharedRefBase::make<AmbientDataCaptureAIDL>();
  if (serviceDisplayConfigAIDL == nullptr || serviceAmbientDataCaptureAIDL == nullptr) {
    return 0;
  }

  FuzzedDataProvider provider(data, size);
  uint32_t index = provider.ConsumeIntegralInRange<uint32_t>(0, 1);

  if (index == 0) {
    fuzzService(serviceDisplayConfigAIDL->asBinder().get(), std::move(provider));
  } else if (index == 1) {
    fuzzService(serviceAmbientDataCaptureAIDL->asBinder().get(), std::move(provider));
  }
#else

  fuzzService(serviceDisplayConfigAIDL->asBinder().get(), FuzzedDataProvider(data, size));
#endif

  return 0;
}
