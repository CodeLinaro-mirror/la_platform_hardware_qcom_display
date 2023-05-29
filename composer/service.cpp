/*
 * Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <hidl/LegacySupport.h>

#include "DisplayConfigAIDL.h"
#include "AidlComposer.h"
#include "QtiComposer3Client.h"

using aidl::vendor::qti::hardware::display::composer3::AidlComposer;
using aidl::vendor::qti::hardware::display::composer3::QtiComposer3Client;
using aidl::vendor::qti::hardware::display::config::DisplayConfigAIDL;
using android::ProcessState;
using android::sp;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

int main(int, char **) {
  ALOGI("Creating Display HW Composer HAL");

  // TODO(user): double-check for SCHED_FIFO logic
  // the conventional HAL might start binder services
  ProcessState::initWithDriver("/dev/vndbinder");
  sp<ProcessState> ps(ProcessState::self());

  // Explicitly set thread priority in case it isn't inherited correctly
  setpriority(PRIO_PROCESS, 0, sdm::kThreadPriorityUrgent);

  // same as SF main thread
  struct sched_param param = {0};
  param.sched_priority = 2;
  if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
    ALOGE("Couldn't set SCHED_FIFO: %d", errno);
  } else {
    ALOGI("Scheduler priority settings completed");
  }

  ALOGI("Configuring RPC threadpool");
  configureRpcThreadpool(4, true /*callerWillJoin*/);
  ALOGI("Configuring RPC threadpool...done!");

  ALOGI("Registering AidlComposer as a service");
  ABinderProcess_setThreadPoolMaxThreadCount(0);
  ALOGI("Creating AidlComposer extensions(QtiComposer3Client) service");
  std::shared_ptr<QtiComposer3Client> qticomposer = ndk::SharedRefBase::make<QtiComposer3Client>();
  std::shared_ptr<AidlComposer> composer = ndk::SharedRefBase::make<AidlComposer>(qticomposer);
  const std::string instance = std::string() + AidlComposer::descriptor + "/default";
  if (!composer->asBinder().get()) {
    ALOGW("AidlComposer's binder is null");
  }

  ndk::SpAIBinder composerBinder = composer->asBinder();
  ALOGI("Registering QtiComposer3Client as an AidlComposer extension service");
  binder_status_t status =
      AIBinder_setExtension(composerBinder.get(), qticomposer->asBinder().get());
  if (status != STATUS_OK) {
    ALOGW("Failed to register QtiComposer3Client as an AidlComposer extension service (status:%d)",
          status);
  } else {
    ALOGI("Successfully registered QtiComposer3Client as an AidlComposer extension service");
  }

  status = AServiceManager_addService(composerBinder.get(), instance.c_str());
  if (status != STATUS_OK) {
    ALOGW("Failed to register AidlComposer as a service (status:%d)", status);
  } else {
    ALOGI("Successfully registered AidlComposer as a service");
  }

  ALOGI("Registering DisplayConfig AIDL as a service");
  std::shared_ptr<DisplayConfigAIDL> displayConfig = ndk::SharedRefBase::make<DisplayConfigAIDL>();
  const std::string instance2 = std::string() + DisplayConfigAIDL::descriptor + "/default";
  if (!displayConfig->asBinder().get()) {
    ALOGW("Display Config AIDL's binder is null");
  }

  status = AServiceManager_addService(displayConfig->asBinder().get(), instance2.c_str());
  if (status != STATUS_OK) {
    ALOGW("Failed to register DisplayConfig AIDL as a service (status:%d)", status);
  } else {
    ALOGI("Successfully registered DisplayConfig AIDL as a service");
  }

  ps->setThreadPoolMaxThreadCount(4);
  ps->startThreadPool();
  ALOGI("ProcessState initialization completed");

  ALOGI("Joining RPC threadpool...");
  ABinderProcess_setThreadPoolMaxThreadCount(5);
  ABinderProcess_startThreadPool();
  ABinderProcess_joinThreadPool();

  return 0;
}
