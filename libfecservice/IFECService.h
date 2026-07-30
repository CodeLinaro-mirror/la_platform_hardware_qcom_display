/*
 Copyright (C) 2010 The Android Open Source Project
 Copyright (C) 2012-2014, 2016-2019, The Linux Foundation. All rights reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
===========================================================================*/
/*
 * ​​​​​Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ANDROID_IFECSERVICE_H
#define ANDROID_IFECSERVICE_H

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>

class IFECService : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(FECService);
  enum {
    COMMAND_LIST_START = android::IBinder::FIRST_CALL_TRANSACTION,
    LIST_ALL_INSTALLED_FEATURES = 2,
    VALIDATE_LICENSE = 3,
    ENABLE_ALL_FEATURES = 4,
    LIST_ALL_ENABLED_FEATURES = 5,
    VALIDATE_AND_ENABLE = 6,
    COMMAND_LIST_END = 100,
  };

  enum {
    ALL_MODULES = 0,
    DISPLAY_MODULE,
    MAX_MODULE,
  };

  // Generic function to handle binder commands
  // The type of command decides how the data is parceled
  virtual android::status_t dispatch(uint32_t command, const android::Parcel *inParcel,
                                     android::Parcel *outParcel) = 0;
};

// ----------------------------------------------------------------------------

class BnFECService : public android::BnInterface<IFECService> {
 public:
  virtual android::status_t onTransact(uint32_t code, const android::Parcel &data,
                                       android::Parcel *reply, uint32_t flags = 0);
};

#endif  // ANDROID_IFECSERVICE_H