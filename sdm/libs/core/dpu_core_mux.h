/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __DPU_CORE_MUX_H__
#define __DPU_CORE_MUX_H__

#include <map>
#include <vector>
#include <private/hw_info_types.h>
#include "hw_interface.h"
#include "hw_info_interface.h"

namespace sdm {

class DPUCoreMux {
 public:
  static DPUCoreMux *CreateCoreMux(DisplayId display_id, DisplayType type,
                                   std::vector<HWInfoInterface*> hw_info_intf,
                                   BufferAllocator *buffer_allocator);
  static DisplayError DestroyCoreMux(DPUCoreMux **intf);

  explicit DPUCoreMux(DisplayId display_id) : display_id_(display_id) {}

  DisplayError Init(DisplayId display_id, DisplayType type,
                    std::vector<HWInfoInterface*> hw_info_intf,
                    BufferAllocator *buffer_allocator);

  // Only destroys SECONDARY core HWInterfaces.
  // PRIMARY (core_ids_[0]) is owned by DisplayBase via hw_intf_ and is not touched here.
  DisplayError DeInit();

  DisplayError GetDisplayId(int32_t *display_id);
  void GetHWInterface(HWInterface **intf);

 private:
  std::map<uint32_t, HWInterface*> hw_intf_;
  std::vector<uint32_t> core_ids_;
  DisplayId display_id_ = {};
};

}  // namespace sdm
#endif  // __DPU_CORE_MUX_H__
