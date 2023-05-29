/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include "comp_test_bnd_service.h"
#include "composer_test_service.h"

using namespace android;

// ----------------------------------------------------------------------------
int CompTestBndService::Init(sdm::ComposerTestService *comp_test_service) {
  if (!comp_test_service) {
    return -1;
  }

  android::sp<CompTestBndService> service = new CompTestBndService(comp_test_service);
  if (!service) {
    ALOGE("CompTestBndService create failed");
    return -1;
  }

  android::sp<android::IServiceManager> sm = android::defaultServiceManager();
  sm->addService(android::String16("comp_test.qservice"), service);
  if (sm->checkService(android::String16("comp_test.qservice")) != nullptr) {
    ALOGI("adding comp_test.qservice succeeded");
    return 0;
  } else {
    ALOGE("adding comp_test.qservice failed");
    return -1;
  }
}

status_t CompTestBndService::dispatch(uint32_t command, const Parcel* inParcel,
                                      Parcel* outParcel) {
  status_t err = (status_t) FAILED_TRANSACTION;
  IPCThreadState* ipc = IPCThreadState::self();
  //Rewind parcel in case we're calling from the same process
  bool sameProcess = (ipc->getCallingPid() == getpid());
  if(sameProcess) {
    inParcel->setDataPosition(0);
  }
  err = comp_test_service_->CommandHandler(command, inParcel, outParcel);
  //Rewind parcel in case we're calling from the same process
  if (sameProcess) {
    outParcel->setDataPosition(0);
  }

  return err;
}
