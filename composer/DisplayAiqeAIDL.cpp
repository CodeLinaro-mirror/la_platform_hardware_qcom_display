/*
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <DisplayAiqeAIDL.h>

#include "display_properties.h"

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace aiqe {

using namespace ndk;

DisplayAiqeAIDL::DisplayAiqeAIDL() {
  sdm::SDMInterfaceFactory *sdm_factory = sdm::GetSDMInterfaceFactory();

  aiqe_intf_ = sdm_factory->CreateAiqeIntf();
  sideband_ = sdm_factory->CreateSideBandIntf();
}

DisplayAiqeAIDL::DisplayAiqeAIDL(sdm::SDMInterfaceFactory *sdm_factory) {
  aiqe_intf_ = sdm_factory->CreateAiqeIntf();
  sideband_ = sdm_factory->CreateSideBandIntf();
}

bool DisplayAiqeAIDL::isSupported() {
  int enable = 0;

  if (!sideband_) {
    ALOGE("Unable to query display property states");
    return false;
  }

  sideband_->GetProperty(AIQE_SSRC_ENABLE, &enable);
  if (enable) {
    if (!aiqe_intf_) {
      ALOGE("SDM does not support the AIQE interface");
      return false;
    }

    return true;
  } else {
    ALOGI("No known features requiring AIQE interface");
    return false;
  }
}

ScopedAStatus DisplayAiqeAIDL::setSsrcMode(int32_t in_disp_id, const std::string &in_mode_name) {
  if (aiqe_intf_) {
    int rc = aiqe_intf_->SetSsrcMode(in_disp_id, in_mode_name);
    if (rc) {
      ALOGE("%s: Unable to set SSRC Mode '%s'", __FUNCTION__, in_mode_name.c_str());
      return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }

    return ScopedAStatus::ok();
  } else {
    ALOGE("%s: Unable to set SSRC mode. Interface initalized with bad session instance",
          __FUNCTION__);
    return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
  }
}

ndk::ScopedAStatus DisplayAiqeAIDL::enableCopr(int32_t disp_id, bool enable) {
  return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
}

ndk::ScopedAStatus DisplayAiqeAIDL::getCoprStats(int32_t disp_id,
                                                 std::vector<int32_t> *_aidl_return) {
  return ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
}

}  // End of namespace aiqe
}  // End of namespace display
}  // End of namespace hardware
}  // End of namespace qti
}  // End of namespace vendor
}  // End of namespace aidl
