/*
 Copyright (C) 2010 The Android Open Source Project
 Copyright (C) 2012-2016, The Linux Foundation. All rights reserved.

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

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#if defined(__ANDROID__)
#include <cutils/android_filesystem_config.h>
#else
#define AID_ROOT 0
#define AID_SYSTEM 1000
#define AID_GRAPHICS 1003
#define AID_AUDIO 1004
#define AID_CAMERA 1007
#define AID_MEDIA 1013
#define AID_MEDIA_CODEC 1016
#define AID_CAMERASERVER 2800
#endif
#include <utils/Errors.h>
#include <IFECService.h>

#define FEC_SERVICE_DEBUG 0

using namespace android;

// ---------------------------------------------------------------------------

class BpFECService : public BpInterface<IFECService> {
 public:
  explicit BpFECService(const sp<IBinder> &impl) : BpInterface<IFECService>(impl) {}

  virtual android::status_t dispatch(uint32_t command, const Parcel *inParcel, Parcel *outParcel) {
    ALOGD_IF(FEC_SERVICE_DEBUG, "%s: dispatch in:%p", __FUNCTION__, inParcel);
    status_t err = (status_t)android::FAILED_TRANSACTION;
    Parcel data;
    Parcel *reply = outParcel;
    data.writeInterfaceToken(IFECService::getInterfaceDescriptor());
    if (inParcel && inParcel->dataSize() > 0)
      data.appendFrom(inParcel, 0, inParcel->dataSize());
    err = remote()->transact(command, data, reply);
    return err;
  }
};

IMPLEMENT_META_INTERFACE(FECService, "android.IFECService");

// ----------------------------------------------------------------------

status_t BnFECService::onTransact(uint32_t code, const Parcel &data, Parcel *reply,
                                  uint32_t flags) {
  ALOGD_IF(FEC_SERVICE_DEBUG, "%s: code: %d", __FUNCTION__, code);
  // IPC should be from certain processes only
  IPCThreadState *ipc = IPCThreadState::self();
  if (!ipc) {
    ALOGE("Issue in getting IPCThreadState");
    return BAD_VALUE;
  }
  const int callerPid = ipc->getCallingPid();
  const int callerUid = ipc->getCallingUid();

  const bool permission =
      (callerUid == AID_MEDIA || callerUid == AID_GRAPHICS || callerUid == AID_ROOT ||
       callerUid == AID_CAMERASERVER || callerUid == AID_AUDIO || callerUid == AID_SYSTEM ||
       callerUid == AID_MEDIA_CODEC);

  if (code > COMMAND_LIST_START && code < COMMAND_LIST_END) {
    if (!permission) {
      ALOGE("feature_enabler_client service access denied: command=%d pid=%d uid=%d", code,
            callerPid, callerUid);
      return PERMISSION_DENIED;
    }
    CHECK_INTERFACE(IFECService, data, reply);
    dispatch(code, &data, reply);
    return NO_ERROR;
  } else {
    return BBinder::onTransact(code, data, reply, flags);
  }
}
