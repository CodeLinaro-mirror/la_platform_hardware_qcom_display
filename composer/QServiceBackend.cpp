/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "QServiceBackend.h"

#define __CLASS__ "QServiceBackend"

namespace sdm {

QServiceBackend *QServiceBackend::GetInstance() {
  static QServiceBackend *qservice_ = ::new QServiceBackend();
  return qservice_;
}

void QServiceBackend::Init() {
  const char *qservice_name = "display.qservice";

  // Start QService and connect to it.
  DLOGI("Initializing QService");
  qService::QService::init();
  DLOGI("Initializing QService...done!");

  DLOGI("Getting IQService");
  android::sp<qService::IQService> iqservice = android::interface_cast<qService::IQService>(
      android::defaultServiceManager()->checkService(android::String16(qservice_name)));
  DLOGI("Getting IQService...done!");

  if (iqservice.get()) {
    iqservice->connect(android::sp<qClient::IQClient>(this));
    qservice_ = reinterpret_cast<qService::QService *>(iqservice.get());
    DLOGI("Acquired %s", qservice_name);
  } else {
    DLOGE("Failed to acquire %s", qservice_name);
    return;
  }

  SDMInterfaceFactory *sdm_factory = nullptr;
  sdm_factory = GetSDMInterfaceFactory();
  sideband_ = sdm_factory->CreateSideBandIntf();
}

android::status_t QServiceBackend::notifyCallback(uint32_t command, const android::Parcel *input_parcel,
                                             android::Parcel *output_parcel) {
  if (!sideband_) {
    DLOGE("SDM Sideband intf is not available");
    return -1;
  }

  HWCParcel sdm_input_parcel(input_parcel);
  HWCParcel sdm_output_parcel(output_parcel);

  DLOGI("calling qservice command %d", command);
  auto err = sideband_->NotifyCallback(command, &sdm_input_parcel, &sdm_output_parcel);

  if (err != kErrorNone) {
    DLOGW("notifyCallback failed error %d:", err);
    return -1;
  }

  return 0;
}

void QServiceBackend::OnCECMessageReceived(char *message, int len) {
  if (qservice_) {
    qservice_->onCECMessageReceived(message, len);
  } else {
    DLOGW("Qservice instance not available.");
  }
}

void QServiceBackend::OnHdmiHotplug(bool connected) {
  if (qservice_) {
    qservice_->onHdmiHotplug(connected);
  } else {
    DLOGW("QService instance not available");
  }
}

} // namespace sdm
