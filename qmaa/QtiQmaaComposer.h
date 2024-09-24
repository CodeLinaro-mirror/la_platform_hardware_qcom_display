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
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QTIQMAACOMPOSER_H__
#define __QTIQMAACOMPOSER_H__

#include <QtiQmaaComposerClient.h>

#include <aidl/android/hardware/graphics/composer3/BnComposer.h>
#include <log/log.h>
#include <utils/Mutex.h>
#include <memory>

#include <unordered_set>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace composer3 {

using ::aidl::android::hardware::graphics::composer3::BnComposer;
using ::aidl::android::hardware::graphics::composer3::Capability;
using ndk::ScopedAStatus;
using ndk::SpAIBinder;

class QtiComposer : public BnComposer {
 public:
  QtiComposer();
  virtual ~QtiComposer();

  ScopedAStatus createClient(std::shared_ptr<IComposerClient> *aidl_return) override;
  ScopedAStatus getCapabilities(std::vector<Capability> *aidl_return) override;

  binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

 protected:
  SpAIBinder createBinder() override;

 private:
  bool waitForClientDestroyedLocked(std::unique_lock<std::mutex> &lock);
  void onClientDestroyed();

  std::mutex mClientMutex;
  bool mClientAlive GUARDED_BY(mClientMutex) = false;
  std::condition_variable mClientDestroyedCondition;
};

}  // namespace composer3
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace aidl

#endif  // __QTIQMAACOMPOSER_H__
