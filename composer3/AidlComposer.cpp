/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "AidlComposer.h"
#include "android/binder_auto_utils.h"
#include <android/binder_ibinder_platform.h>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

AidlComposer::AidlComposer(const shared_ptr<QtiComposer3Client> &extensions)
    : extensions_(extensions), hwc_session_(HWCSession::GetInstance()) {
  auto error = hwc_session_->Init();
  if (error) {
    ALOGE("Failed to get HWComposer instance");
  } else {
    ALOGI("Successfully initialized HWCSession, creating AidlComposer");
  }
}

AidlComposer::~AidlComposer() {
  hwc_session_->Deinit();
}

ScopedAStatus AidlComposer::createClient(std::shared_ptr<IComposerClient> *aidl_return) {
  std::unique_lock<std::mutex> lock(mClientMutex);
  if (!waitForClientDestroyedLocked(lock)) {
    *aidl_return = nullptr;
    return TO_BINDER_STATUS(INT32(Error::NoResources));
  }

  composer_client_ = ndk::SharedRefBase::make<AidlComposerClient>();
  if (!composer_client_ || !composer_client_->init()) {
    *aidl_return = nullptr;
    return TO_BINDER_STATUS(INT32(Error::NoResources));
  }

  if (extensions_) {
    extensions_->init(composer_client_);
  }

  auto clientDestroyed = [this]() { onClientDestroyed(); };
  composer_client_->setOnClientDestroyed(clientDestroyed);

  mClientAlive = true;
  *aidl_return = composer_client_;

  return ScopedAStatus::ok();
}

binder_status_t AidlComposer::dump(int fd, const char ** /*args*/, uint32_t /*numArgs*/) {
  uint32_t len;
  std::string output;

  hwc_session_->Dump(&len, nullptr);
  output.resize(len + 1);

  hwc_session_->Dump(&len, output.data());

  output[len] = '\0';
  write(fd, output.c_str(), output.size());

  return STATUS_OK;
}

ScopedAStatus AidlComposer::getCapabilities(std::vector<Capability> *aidl_return) {
  const std::array<Capability, 2> all_caps = {{
      Capability::SIDEBAND_STREAM,
      Capability::SKIP_VALIDATE,
  }};

  uint32_t count = 0;
  // Capability::SKIP_CLIENT_COLOR_TRANSFORM is no longer supported as client queries per display
  // capabilities from AidlComposerClient::getDisplayCapabilities
  hwc_session_->GetCapabilities(&count, nullptr);

  if (!count) {
    return TO_BINDER_STATUS(INT32(Error::Unsupported));
  }

  std::vector<int32_t> composer_caps(count);
  hwc_session_->GetCapabilities(&count, composer_caps.data());
  composer_caps.resize(count);

  std::unordered_set<Capability> capabilities;
  capabilities.reserve(count);
  for (auto cap : composer_caps) {
    capabilities.insert(static_cast<Capability>(cap));
  }

  std::vector<Capability> caps;
  for (auto cap : all_caps) {
    if (capabilities.count(static_cast<Capability>(cap)) > 0) {
      caps.push_back(cap);
    }
  }

  hidl_vec<Capability> caps_reply;
  caps_reply.setToExternal(caps.data(), caps.size());

  *aidl_return = caps_reply;

  return ScopedAStatus::ok();
}

bool AidlComposer::waitForClientDestroyedLocked(std::unique_lock<std::mutex> &lock) {
  if (mClientAlive) {
    using namespace std::chrono_literals;

    // In surface flinger we delete a composer client on one thread and
    // then create a new client on another thread. Although surface
    // flinger ensures the calls are made in that sequence (destroy and
    // then create), sometimes the calls land in the composer service
    // inverted (create and then destroy). Wait for a brief period to
    // see if the existing client is destroyed.
    ALOGI("waiting for previous client to be destroyed");
    mClientDestroyedCondition.wait_for(lock, 1s, [this]() -> bool { return !mClientAlive; });
    if (mClientAlive) {
      ALOGE("previous client was not destroyed");
    }
  }

  return !mClientAlive;
}

void AidlComposer::onClientDestroyed() {
  std::lock_guard<std::mutex> lock(mClientMutex);
  mClientAlive = false;
  mClientDestroyedCondition.notify_all();
}

SpAIBinder AidlComposer::createBinder() {
  auto binder = BnComposer::createBinder();
  AIBinder_setInheritRt(binder.get(), true);
  return binder;
}

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl
