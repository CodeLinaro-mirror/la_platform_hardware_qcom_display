/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __COMPOSER_TEST_SERVICE_H__
#define __COMPOSER_TEST_SERVICE_H__

#include <binder/Parcel.h>

#include "core/core_interface.h"
#include "sdm_comp_buffer_allocator.h"
#include "sdm_comp_buffer_sync_handler.h"
#include "sdm_comp_display_builtin.h"
#include "sdm_comp_types.h"
#include "core/sdm_types.h"

namespace sdm {

class ComposerTestService {
 public:
  int CommandHandler(uint32_t command, const android::Parcel *input_parcel,
                     android::Parcel *output_parcel);

 private:
  int Init();
  int Deinit();
  int DisplayContent();
  int RunTest();
  int SetQSyncMode(const android::Parcel *input_parcel);

  CoreInterface *core_intf_ = nullptr;
  SDMCompBufferAllocator buffer_allocator_;
  SDMCompDisplayBuiltIn *display_builtin_ = nullptr;
  BufferHandle buffer_handle_[5];
  QSyncMode qsync_mode_ = kQSyncModeNone;
};

}  // namespace sdm

#endif  // __COMPOSER_TEST_SERVICE_H__
