/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <aidl/vendor/qti/hardware/display/composer3/BnQtiComposer3Client.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <binder/Status.h>
#include <log/log.h>
#include <utils/locker.h>

#include "aidl/android/hardware/graphics/composer3/DisplayCommand.h"
#include "hwc_session.h"
#include "AidlComposerClient.h"

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

using aidl::android::hardware::graphics::composer3::CommandResultPayload;
using aidl::android::hardware::graphics::composer3::DisplayCommand;
using aidl::vendor::qti::hardware::display::composer3::AidlComposerClient;
using aidl::vendor::qti::hardware::display::composer3::QtiDisplayCommand;
using aidl::vendor::qti::hardware::display::composer3::QtiDrawMethod;
using ::android::binder::Status;
using ndk::ScopedAStatus;
using sdm::Display;
using sdm::HWCSession;
using sdm::HWC3::Error;

class QtiComposer3Client : public BnQtiComposer3Client {
 public:
  QtiComposer3Client();

  ScopedAStatus qtiExecuteCommands(const std::vector<DisplayCommand> &in_commands,
                                   const std::vector<QtiDisplayCommand> &in_qtiCommands,
                                   std::vector<CommandResultPayload> *_aidl_return);
  ScopedAStatus qtiTryDrawMethod(int64_t in_display, QtiDrawMethod in_drawMethod);
  ScopedAStatus init(const std::weak_ptr<AidlComposerClient> &composer_client);

 protected:
  SpAIBinder createBinder() override;

 private:
  std::weak_ptr<AidlComposerClient> composer_client_;
  sdm::HWCSession *hwc_session_;
};

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
