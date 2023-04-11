/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __CWB_TEST_INTERFACE_H__
#define __CWB_TEST_INTERFACE_H__

#include "core/buffer_allocator.h"
#include "config/config_defs.h"

namespace sdm {

#define CWB_TEST_LIB_NAME "cwb-test.so"

struct CWBConfig {
  /* target display */
  DisplayConfig::DisplayType disp = DisplayConfig::DisplayType::kPrimary;

  /* flag to specify DSPP output */
  bool post_processed = false;

  /* format of the output buffer */
  LayerBufferFormat format = kFormatRGBA8888;

  /* CWB ROI */
  DisplayConfig::Rect cwb_roi = {};

  /* frequency in milli seconds at which CWB is to be triggered */
  uint64_t trigger_frequency = 100;
};

class CWBTestInterface {
 public:
  /* configure/ reconfigure the CWB test */
  virtual int ConfigureCWB(struct CWBConfig cwb_config) = 0;

  /* stop CWB test */
  virtual int StopTest(DisplayConfig::DisplayType disp) = 0;

 protected:
  virtual ~CWBTestInterface() { }
};

/* function addresses queried at runtime using ::dlsym() */
typedef int (*CreateCWBTestInterface)(BufferAllocator *buffer_allocator,
                                      CWBTestInterface **cwb_test_interface);

typedef void (*DestroyCWBTestInterface)(CWBTestInterface *cwb_test_interface);

}  // namespace sdm

#endif  // __CWB_TEST_INTERFACE_H__