/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "QtiComposer3Client.h"
#include "AidlComposerClient.h"
#include "android/binder_status.h"
#include "android/binder_auto_utils.h"
#include <android/binder_ibinder_platform.h>

using ::aidl::android::hardware::common::NativeHandle;
using sdm::Locker;

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

QtiComposer3Client::QtiComposer3Client() {}

ScopedAStatus QtiComposer3Client::init(const std::weak_ptr<AidlComposerClient> &composer_client) {
  composer_client_ = composer_client;

  SDMInterfaceFactory *sdm_factory = nullptr;
  sdm_factory = sdm::GetSDMInterfaceFactory();
  lifecycle_ = sdm_factory->CreateLifeCycleIntf();

  return ScopedAStatus::ok();
}

ScopedAStatus QtiComposer3Client::qtiExecuteCommands(
    const std::vector<DisplayCommand> &in_commands,
    const std::vector<QtiDisplayCommand> &in_qtiCommands,
    std::vector<CommandResultPayload> *_aidl_return) {
  auto composer_client = composer_client_.lock();

  if (composer_client) {
    auto status = ScopedAStatus::ok();
    if (!in_qtiCommands.empty()) {
      status =
          composer_client->executeQtiExtendedCommands(in_commands, in_qtiCommands, _aidl_return);
    } else if (!in_commands.empty()) {
      status = composer_client->executeCommands(in_commands, _aidl_return);
    }
    return std::move(status);
  }
  return TO_BINDER_STATUS(INT32(Error::NoResources));
}

ScopedAStatus QtiComposer3Client::qtiTryDrawMethod(int64_t in_display,
                                                   QtiDrawMethod in_drawMethod) {
  if (lifecycle_) {
    sdm::DisplayDrawMethod draw_method = sdm::kDrawDefault;
    if (in_drawMethod == QtiDrawMethod::UNIFIED_DRAW) {
      draw_method = sdm::kDrawUnified;
    }
    auto error = lifecycle_->TryDrawMethod(in_display, draw_method);
    return TO_BINDER_STATUS(INT32(error));
  }
  return ScopedAStatus::ok();
}

#ifdef TARGET_USES_LSR
ScopedAStatus QtiComposer3Client::qtiSetDisplayDeviceConfig(
    int64_t in_display, const QtiDisplayDeviceConfig &in_displayDeviceConfig) {
  if (lifecycle_) {
    sdm::SDMDisplayDeviceConfig display_device_config;
    GetSDMDisplayDeviceConfig(in_displayDeviceConfig, display_device_config);

    auto error = lifecycle_->SetDisplayDeviceConfig(in_display, display_device_config);
    return TO_BINDER_STATUS(INT32(error));
  }
  return ScopedAStatus::ok();
}

void QtiComposer3Client::GetSDMDisplayDeviceConfig(const QtiDisplayDeviceConfig &qti_device_config,
                                                   sdm::SDMDisplayDeviceConfig &sdm_device_config) {
  for (int i = 0; i < qti_device_config.projectionMatrix.size(); i++) {
    for (int row = 0; row < qti_device_config.projectionMatrix[i].prjMatrix.size(); row++) {
      std::copy(qti_device_config.projectionMatrix[i].prjMatrix[row].begin(),
                qti_device_config.projectionMatrix[i].prjMatrix[row].end(),
                sdm_device_config.projectionMatrix[i].prjMatrix[row]);
    }
    sdm::SDMLayerOrientation sdm_orientation{
        qti_device_config.rotation[i].x, qti_device_config.rotation[i].y,
        qti_device_config.rotation[i].z, qti_device_config.rotation[i].w};
    sdm_device_config.rotation[i] = sdm_orientation;
  }
  std::copy(qti_device_config.gamma.begin(), qti_device_config.gamma.end(),
            sdm_device_config.gamma);
  std::copy(std::begin(qti_device_config.calibrationFileStr),
            std::end(qti_device_config.calibrationFileStr),
            std::begin(sdm_device_config.calibrationFileStr));
}
#endif

SpAIBinder QtiComposer3Client::createBinder() {
  auto binder = BnQtiComposer3Client::createBinder();
  AIBinder_setInheritRt(binder.get(), true);
  return binder;
}

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
