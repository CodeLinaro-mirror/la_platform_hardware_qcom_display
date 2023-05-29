/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#include <cutils/android_filesystem_config.h>
#include <utils/Errors.h>
#include <ICompTestBndService.h>

using namespace android;

// ---------------------------------------------------------------------------

class BpCompTestBndService : public BpInterface<ICompTestBndService> {
 public:
  explicit BpCompTestBndService(const sp<IBinder>& impl)
      : BpInterface<ICompTestBndService>(impl) {}

  virtual android::status_t dispatch(uint32_t command, const Parcel* inParcel,
                                           Parcel* outParcel) {
    ALOGI("%s: dispatch in:%p", __FUNCTION__, inParcel);
    status_t err = (status_t) android::FAILED_TRANSACTION;
    Parcel data;
    Parcel *reply = outParcel;
    data.writeInterfaceToken(ICompTestBndService::getInterfaceDescriptor());
    if (inParcel && inParcel->dataSize() > 0)
      data.appendFrom(inParcel, 0, inParcel->dataSize());
    err = remote()->transact(command, data, reply);
    return err;
  }
};

IMPLEMENT_META_INTERFACE(CompTestBndService, "android.ICompTestBndService");

// ----------------------------------------------------------------------

status_t BnCompTestBndService::onTransact(
  uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  ALOGI("%s: code: %d", __FUNCTION__, code);
  // IPC should be from certain processes only
  IPCThreadState* ipc = IPCThreadState::self();
  const int callerPid = ipc->getCallingPid();
  const int callerUid = ipc->getCallingUid();

  const bool permission = (callerUid == AID_MEDIA || callerUid == AID_GRAPHICS ||
                           callerUid == AID_ROOT || callerUid == AID_CAMERASERVER ||
                           callerUid == AID_AUDIO || callerUid == AID_SYSTEM ||
                           callerUid == AID_MEDIA_CODEC);

  if (code > COMMAND_LIST_START && code < COMMAND_LIST_END) {
    if(!permission) {
      ALOGE("composer test service access denied: command=%d pid=%d uid=%d",
             code, callerPid, callerUid);
      return PERMISSION_DENIED;
    }
    CHECK_INTERFACE(ICompTestBndService, data, reply);
    dispatch(code, &data, reply);
    return NO_ERROR;
  } else {
    return BBinder::onTransact(code, data, reply, flags);
  }
}
