/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <map>
#include <vector>
#include <utils/debug.h>
#include "dpu_core_mux.h"

#define __CLASS__ "DPUCoreMux"

namespace sdm {

DPUCoreMux *DPUCoreMux::CreateCoreMux(DisplayId display_id, DisplayType type,
                                      std::vector<HWInfoInterface*> hw_info_intf,
                                      BufferAllocator *buffer_allocator) {
  if (hw_info_intf.empty()) {
    DLOGE("DPUCoreMux Init: empty hw_info_intf for display 0x%x", display_id.GetDisplayId());
    return nullptr;
  }

  DPUCoreMux *core_mux = new DPUCoreMux(display_id);
  if (!core_mux) return nullptr;

  auto error = core_mux->Init(display_id, type, hw_info_intf, buffer_allocator);
  if (error != kErrorNone) {
    DPUCoreMux::DestroyCoreMux(&core_mux);
    return nullptr;
  }
  return core_mux;
}

DisplayError DPUCoreMux::DestroyCoreMux(DPUCoreMux **intf) {
  if (!intf || !*intf) {
    return kErrorParameters;
  }
  (*intf)->DeInit();
  delete *intf;
  *intf = nullptr;
  return kErrorNone;
}

DisplayError DPUCoreMux::Init(DisplayId display_id, DisplayType type,
                              std::vector<HWInfoInterface*> hw_info_intf,
                              BufferAllocator *buffer_allocator) {
  if (hw_info_intf.empty()) {
    DLOGE("DPUCoreMux Init: empty hw_info_intf for display 0x%x", display_id.GetDisplayId());
    return kErrorParameters;
  }

  for (uint32_t i = 0; i < hw_info_intf.size(); i++) {
    HWInterface *hw = nullptr;
    uint32_t core_id = hw_info_intf[i]->GetCoreId();

    // If display_id is -1 (unset), pass -1 so HWDeviceDRM::Init() uses
    // RegisterDisplay(by type). Otherwise extract the raw DRM connector ID
    // for this core using GetConnId().
    int32_t conn_id = (display_id.GetDisplayId() == (uint32_t)-1)
                      ? -1
                      : (int32_t)display_id.GetConnId(core_id);
    auto error = HWInterface::Create(conn_id, type,
                                     hw_info_intf[i], buffer_allocator, &hw);
    if (error != kErrorNone) {
      DLOGE("HWInterface::Create failed for core_id=%u error=%d", core_id, error);
      // Destroy all successfully created interfaces before returning error.
      // Since none have been exposed yet (caller hasn't called GetHWInterface),
      // all entries in hw_intf_ are owned here and must be cleaned up.
      for (auto &entry : hw_intf_) {
        HWInterface::Destroy(entry.second);
      }
      hw_intf_.clear();
      core_ids_.clear();
      return error;
    }

    hw_intf_.insert(std::make_pair(core_id, hw));
    core_ids_.push_back(core_id);
  }
  return kErrorNone;
}

DisplayError DPUCoreMux::DeInit() {
  // IMPORTANT: Only destroy SECONDARY core HWInterfaces (index > 0 in core_ids_ vector).
  // The PRIMARY core's HWInterface (core_ids_[0]) is exposed via GetHWInterface() and
  // stored as DisplayBase::hw_intf_. DisplayBase::Deinit() is responsible for its
  // lifecycle via HWInterface::Destroy(hw_intf_). Destroying it here causes a double-free.
  for (uint32_t i = 1; i < core_ids_.size(); i++) {
    HWInterface::Destroy(hw_intf_.at(core_ids_[i]));
  }
  return kErrorNone;
}

void DPUCoreMux::GetHWInterface(HWInterface **intf) {
  // Returns the HWInterface for this display's primary core (core_ids_[0]).
  // For DSI display: core_ids_[0]=0 (card0). For SPI display: core_ids_[0]=1 (card1).
  // DisplayBase stores this in hw_intf_ for all runtime HW calls.
  if (core_ids_.empty()) {
    *intf = nullptr;
    return;
  }
  *intf = hw_intf_.at(core_ids_[0]);
}

DisplayError DPUCoreMux::GetDisplayId(int32_t *display_id) {
  if (core_ids_.empty()) {
    return kErrorUndefined;
  }
  // Gets display ID from this display's primary core HWInterface.
  // core_ids_[0] is core0 for DSI and core1 for SPI, correct in both cases.
  return hw_intf_.at(core_ids_[0])->GetDisplayId(display_id);
}

}  // namespace sdm
