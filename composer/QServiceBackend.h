/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QSERVICE_BACKEND_H__
#define __QSERVICE_BACKEND_H__
#include <QService.h>
#include <sdm_interface_factory.h>

#include "hwc_parcel.h"

namespace sdm {

class QServiceBackend : public qClient::BnQClient {
public:
 static QServiceBackend *GetInstance();
 void Init();
 android::status_t notifyCallback(uint32_t command, const android::Parcel *input_parcel,
                                  android::Parcel *output_parcel);
 void OnCECMessageReceived(char *message, int len);
 void OnHdmiHotplug(bool connected);

private:
  SDMDisplaySideBandIntf *sideband_ = nullptr;
  qService::QService *qservice_ = nullptr;
};

}

#endif