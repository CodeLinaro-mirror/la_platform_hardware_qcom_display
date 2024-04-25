/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include "QtiQmaaComposer.h"
#include "android/binder_auto_utils.h"
#include <android/binder_ibinder_platform.h>
#define INT32(exp) static_cast<int32_t>(exp)

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

QtiQmaaComposer::QtiQmaaComposer() {}

QtiQmaaComposer::~QtiQmaaComposer() {}

ScopedAStatus QtiQmaaComposer::createClient(std::shared_ptr<IComposerClient> *aidl_return) {
  std::unique_lock<std::mutex> lock(mClientMutex);
  if (!waitForClientDestroyedLocked(lock)) {
    *aidl_return = nullptr;
    return TO_BINDER_STATUS(INT32(Error::NoResources));
  }
  auto composer_client = ndk::SharedRefBase::make<QtiQmaaComposerClient>();
  if (!composer_client || !composer_client->init()) {
    *aidl_return = nullptr;
    return TO_BINDER_STATUS(INT32(Error::NoResources));
  }

  auto clientDestroyed = [this]() { onClientDestroyed(); };
  composer_client->setOnClientDestroyed(clientDestroyed);

  mClientAlive = true;
  *aidl_return = composer_client;

  return ScopedAStatus::ok();
}

binder_status_t QtiQmaaComposer::dump(int fd, const char ** /*args*/, uint32_t /*numArgs*/) {
  std::string output;
  output.resize(1);
  output[0] = '\0';

  write(fd, output.c_str(), output.size());

  return STATUS_OK;
}

ScopedAStatus QtiQmaaComposer::getCapabilities(std::vector<Capability> *aidl_return) {
  const std::array<Capability, 2> all_caps = {{
      Capability::SIDEBAND_STREAM,
  }};

  uint32_t count = 0;

  if (!count) {
    return TO_BINDER_STATUS(INT32(Error::Unsupported));
  }

  std::vector<int32_t> composer_caps(count);
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

bool QtiQmaaComposer::waitForClientDestroyedLocked(std::unique_lock<std::mutex> &lock) {
  if (mClientAlive) {
    using namespace std::chrono_literals;

    // In surface flinger we delete a composer client on one thread and
    // then create a new client on another thread. Although surface
    // flinger ensures the calls are made in that sequence (destroy and
    // then create), sometimes the calls land in the composer service
    // inverted (create and then destroy). Wait for a brief period to
    // see if the existing client is destroyed.
    ALOGI("waiting for previous client to be destroyed");
    mClientDestroyedCondition.wait_for(lock, 5s, [this]() -> bool { return !mClientAlive; });
    if (mClientAlive) {
      ALOGE("previous client was not destroyed");
    }
  }

  return !mClientAlive;
}

void QtiQmaaComposer::onClientDestroyed() {
  std::lock_guard<std::mutex> lock(mClientMutex);
  mClientAlive = false;
  mClientDestroyedCondition.notify_all();
}

SpAIBinder QtiQmaaComposer::createBinder() {
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
