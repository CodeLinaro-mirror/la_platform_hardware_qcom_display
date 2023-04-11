/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __ICOMP_TEST_SERVICE_H__
#define __ICOMP_TEST_SERVICE_H__

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>

class ICompTestBndService : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(CompTestBndService);
  enum {
    COMMAND_LIST_START = android::IBinder::FIRST_CALL_TRANSACTION,
    CONFIGURE_CWB_TEST              = 5,
    STOP_CWB_TEST                   = 6,
    COMMAND_LIST_END                = 100,
  };

  // Generic function to handle binder commands
  // The type of command decides how the data is parceled
  virtual android::status_t dispatch(uint32_t command, const android::Parcel* inParcel,
                                     android::Parcel* outParcel) = 0;
};

// ----------------------------------------------------------------------------

class BnCompTestBndService : public android::BnInterface<ICompTestBndService> {
 public:
  virtual android::status_t onTransact( uint32_t code, const android::Parcel& data,
                                        android::Parcel* reply, uint32_t flags = 0);
};

#endif // __ITUI_TEST_SERVICE_H__
