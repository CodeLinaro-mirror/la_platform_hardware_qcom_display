/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
* Changes from Qualcomm Innovation Center are provided under the following license:
* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
  SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <utils/utils.h>
#include <vector>

#include "hw_info_interface.h"
#ifndef TARGET_HEADLESS
#include "drm/hw_info_drm.h"
#endif

#define __CLASS__ "HWInfoInterface"

namespace sdm {

DisplayError HWInfoInterface::Create(std::vector<HWInfoInterface*> *intfs,
                                     std::bitset<8> core_ids) {
  DisplayError error = kErrorNone;

  for (uint32_t i = 0; i < core_ids.size(); i++) {
    if (!core_ids.test(i)) {
      continue;
    }

    HWInfoInterface *hw_info = new HWInfoDRM(i);

    if (!hw_info) {
      DLOGE("Failed allocating HWInfoDRM(%d)", i);
      return kErrorCriticalResource;
    }

    error = hw_info->Init();
    if (error != kErrorNone) {
      delete hw_info;
      hw_info = NULL;
      return intfs->size() ? kErrorNone : error;
    }
#ifndef TARGET_HEADLESS
    intfs->push_back(hw_info);
#else
    intfs->push_back(nullptr);
#endif
  }

  return kErrorNone;
}

DisplayError HWInfoInterface::Destroy(HWInfoInterface *intf) {
  if (intf) {
    delete intf;
  }

  return kErrorNone;
}

}  // namespace sdm
