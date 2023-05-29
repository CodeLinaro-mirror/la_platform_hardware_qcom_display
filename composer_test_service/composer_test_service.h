/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __COMPOSER_TEST_SERVICE_H__
#define __COMPOSER_TEST_SERVICE_H__

#include <binder/Parcel.h>
#include <utils/sys.h>

#include "sdm_comp_buffer_allocator.h"
#include "include/cwb_test_interface.h"

namespace sdm {

class ComposerTestService {
 public:
  ~ComposerTestService();
  int CommandHandler(uint32_t command, const android::Parcel *input_parcel,
                     android::Parcel *output_parcel);
  int Init();

 private:
  int ConfigureCWBTest(const android::Parcel *input_parcel);
  int StopCWBTest(const android::Parcel *input_parcel);

  SDMCompBufferAllocator buffer_allocator_;
  DynLib cwb_test_lib_;
  CWBTestInterface *cwb_test_intf_ = NULL;
  CreateCWBTestInterface create_cwb_test_intf_ = NULL;
  DestroyCWBTestInterface destroy_cwb_test_intf_ = NULL;
};

}  // namespace sdm

#endif  // __COMPOSER_TEST_SERVICE_H__
