/*
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __DISPLAY_AIQE_AIDL_H__
#define __DISPLAY_AIQE_AIDL_H__

#include <aidl/vendor/qti/hardware/display/aiqe/BnDisplayAiqe.h>
#include <binder/Status.h>

#include "sdm_interface_factory.h"
#include "sdm_display_intf_sideband.h"
#include "sdm_display_intf_aiqe.h"

#include <string>

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace aiqe {

class DisplayAiqeAIDL : public BnDisplayAiqe {
 public:
  DisplayAiqeAIDL();
  DisplayAiqeAIDL(sdm::SDMInterfaceFactory *sdm_factory);
  bool isSupported();
  ndk::ScopedAStatus setSsrcMode(int32_t in_disp_id, const std::string &in_mode_name) override;
  ndk::ScopedAStatus enableCopr(int32_t disp_id, bool enable) override;
  ndk::ScopedAStatus getCoprStats(int32_t disp_id, std::vector<int32_t> *_aidl_return) override;
  ndk::ScopedAStatus setABCState(int32_t disp_id, int32_t enable) override;
  ndk::ScopedAStatus setABCReconfig(int32_t disp_id) override;
  ndk::ScopedAStatus setABCMode(int32_t disp_id, const std::string &mode_name) override;

 private:
  std::shared_ptr<sdm::SDMDisplayAiqeIntf> aiqe_intf_;
  std::shared_ptr<sdm::SDMDisplaySideBandIntf> sideband_;
  int ssrc_enable_ = 0;
  int abc_enable_ = 0;
};

}  // End of namespace aiqe
}  // End of namespace display
}  // End of namespace hardware
}  // End of namespace qti
}  // End of namespace vendor
}  // End of namespace aidl

#endif  // __DISPLAY_AIQE_AIDL_H__