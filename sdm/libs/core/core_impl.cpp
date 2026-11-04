/*
* Copyright (c) 2014 - 2016, 2018, 2020 The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted
* provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright notice, this list of
*      conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright notice, this list of
*      conditions and the following disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written
*      permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
* Changes from Qualcomm Technologies, Inc. are provided under the following license:
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <dlfcn.h>
#include <signal.h>
#include <utils/constants.h>
#include <utils/debug.h>
#include <utils/locker.h>
#include <utils/utils.h>
#include <bitset>
#include <vector>

#include "color_manager.h"
#include "core_impl.h"
#include "display_builtin.h"
#include "display_pluggable.h"
#include "display_virtual.h"
#include "hw_info_interface.h"

#define __CLASS__ "CoreImpl"

namespace sdm {

CoreImpl::CoreImpl(BufferAllocator *buffer_allocator,
                   SocketHandler *socket_handler,
                   std::bitset<8> core_ids)
  : buffer_allocator_(buffer_allocator), core_ids_(core_ids),
    socket_handler_(socket_handler) {
}

DisplayError CoreImpl::Init() {
  SCOPE_LOCK(locker_);
  DisplayError error = kErrorNone;

  // Try to load extension library & get handle to its interface.
  if (extension_lib_.Open(EXTENSION_LIBRARY_NAME)) {
    if (!extension_lib_.Sym(CREATE_EXTENSION_INTERFACE_NAME,
                            reinterpret_cast<void **>(&create_extension_intf_)) ||
        !extension_lib_.Sym(DESTROY_EXTENSION_INTERFACE_NAME,
                            reinterpret_cast<void **>(&destroy_extension_intf_))) {
      DLOGE("Unable to load symbols, error = %s", extension_lib_.Error());
      return kErrorUndefined;
    }

    error = create_extension_intf_(EXTENSION_VERSION_TAG, &extension_intf_);
    if (error != kErrorNone) {
      DLOGE("Unable to create interface");
      return error;
    }
  } else {
    DLOGW("Unable to load = %s, error = %s", EXTENSION_LIBRARY_NAME, extension_lib_.Error());
  }

  error = HWInfoInterface::Create(&hw_info_intf_, core_ids_);
  if (error != kErrorNone) {
    goto CleanupOnError;
  }

  if (hw_info_intf_.size() < core_ids_.count()) {
    DLOGW("Requested %zu cores but only %zu initialized successfully",
          core_ids_.count(), hw_info_intf_.size());
  }

  for (auto hw_info : hw_info_intf_) {
    HWResourceInfo hw_resource;
    error = hw_info->GetHWResourceInfo(&hw_resource);
    if (error != kErrorNone) {
      goto CleanupOnError;
    }
    hw_resource_.push_back(hw_resource);
  }

  error = comp_mgr_.Init(hw_resource_, hw_info_intf_, extension_intf_, buffer_allocator_, socket_handler_);
  if (error != kErrorNone) {
    goto CleanupOnError;
  }

  // Color manager is initialized only from the real DPU resource (core0/DSI, hw_resource_[0]).
  // SPI has no color pipe/hw color-processing blocks, so it doesn't need or use a color
  // manager -- this assumes hw_resource_[0] is always core0 (see core_id_mask validation).
  error = ColorManagerProxy::Init(hw_resource_[0]);
  // if failed, doesn't affect display core functionalities.
  if (error != kErrorNone) {
    DLOGW("Unable creating color manager and continue without it.");
  }

  // Populate hw_displays_info_ once.
  {
    hw_displays_info_.clear();
    for (auto hw_info : hw_info_intf_) {
      HWDisplaysInfo display_infos;
      DisplayError err = hw_info->GetDisplaysStatus(&display_infos);
      if (err != kErrorNone) {
        DLOGW("GetDisplaysStatus failed for core %u. Error = %d", hw_info->GetCoreId(), err);
        continue;  // non-fatal: proceed with what we have
      }
      hw_displays_info_.insert(display_infos.begin(), display_infos.end());
    }
    if (hw_displays_info_.empty()) {
      DLOGE("No displays found across all cores.");
      goto CleanupOnError;
    }
  }

  signal(SIGPIPE, SIG_IGN);
  return kErrorNone;

CleanupOnError:
  for (auto hw_info : hw_info_intf_) {
    HWInfoInterface::Destroy(hw_info);
  }
  hw_info_intf_.clear();
  hw_resource_.clear();

  return error;
}

DisplayError CoreImpl::Deinit() {
  SCOPE_LOCK(locker_);

  ColorManagerProxy::Deinit();

  comp_mgr_.Deinit();
  for (auto hw_info : hw_info_intf_) {
    HWInfoInterface::Destroy(hw_info);
  }
  hw_info_intf_.clear();

  return kErrorNone;
}

DisplayError CoreImpl::CreateDisplay(DisplayType type, DisplayEventHandler *event_handler,
                                     DisplayInterface **intf) {
  SCOPE_LOCK(locker_);

  if (!event_handler || !intf) {
    return kErrorParameters;
  }

  DisplayBase *display_base = NULL;

  switch (type) {
    case kBuiltIn:
      display_base = new DisplayBuiltIn(event_handler, hw_info_intf_, buffer_allocator_,
                                        &comp_mgr_);
      break;
    case kPluggable:
      display_base = new DisplayPluggable(event_handler, hw_info_intf_, buffer_allocator_,
                                          &comp_mgr_);
      break;
    case kVirtual:
      display_base = new DisplayVirtual(event_handler, hw_info_intf_, buffer_allocator_,
                                        &comp_mgr_);
      break;
    default:
      DLOGE("Spurious display type %d", type);
      return kErrorParameters;
  }

  if (!display_base) {
    return kErrorMemory;
  }

  DisplayError error = display_base->Init();
  if (error != kErrorNone) {
    delete display_base;
    return error;
  }

  *intf = display_base;
  return kErrorNone;
}

DisplayError CoreImpl::CreateDisplay(int32_t display_id, DisplayEventHandler *event_handler,
                                     DisplayInterface **intf) {
  SCOPE_LOCK(locker_);

  if (!event_handler || !intf) {
    return kErrorParameters;
  }

  auto iter = hw_displays_info_.find(display_id);

  if (iter == hw_displays_info_.end()) {
    DLOGE("Spurious display id %d", display_id);
    return kErrorParameters;
  }

  DisplayBase *display_base = NULL;
  DisplayType display_type = iter->second.display_type;

  // Build sub-vector of hw_info interfaces relevant to each display's cores.
  std::vector<HWInfoInterface*> hw_info_for_display;
  if (display_id == -1) {
    // display_id is not yet resolved (e.g. virtual display before HWInterface::Create()
    // assigns one) -- DisplayId::GetCoreIdMap() would decode -1 as core_id_map = 0, not
    // "all cores", so hand every core down and let the display's Init() pick the one it needs.
    hw_info_for_display = hw_info_intf_;
  } else {
    // display_id is encoded (DisplayId format), so decode core_id_map directly.
    DisplayId disp_id((uint32_t)display_id);
    uint32_t core_id_map = disp_id.GetCoreIdMap();
    for (auto info_intf : hw_info_intf_) {
      if ((core_id_map >> info_intf->GetCoreId()) & 1) {
        hw_info_for_display.push_back(info_intf);
      }
    }
  }

  switch (display_type) {
    case kBuiltIn:
      display_base = new DisplayBuiltIn(display_id, event_handler, hw_info_for_display,
                                        buffer_allocator_, &comp_mgr_);
      break;
    case kPluggable:
      display_base = new DisplayPluggable(display_id, event_handler, hw_info_for_display,
                                          buffer_allocator_, &comp_mgr_);
      break;
    case kVirtual:
      display_base = new DisplayVirtual(display_id, event_handler, hw_info_for_display,
                                        buffer_allocator_, &comp_mgr_);
      break;
    default:
      DLOGE("Spurious display type %d", display_type);
      return kErrorParameters;
  }

  if (!display_base) {
    return kErrorMemory;
  }

  DisplayError error = display_base->Init();
  if (error != kErrorNone) {
    delete display_base;
    return error;
  }

  *intf = display_base;

  return kErrorNone;
}

DisplayError CoreImpl::DestroyDisplay(DisplayInterface *intf) {
  SCOPE_LOCK(locker_);

  if (!intf) {
    return kErrorParameters;
  }

  DisplayBase *display_base = static_cast<DisplayBase *>(intf);
  display_base->Deinit();
  delete display_base;

  return kErrorNone;
}

DisplayError CoreImpl::SetMaxBandwidthMode(HWBwModes mode) {
  SCOPE_LOCK(locker_);

  return comp_mgr_.SetMaxBandwidthMode(mode);
}

DisplayError CoreImpl::GetFirstDisplayInterfaceType(HWDisplayInterfaceInfo *hw_disp_info) {
  SCOPE_LOCK(locker_);
  if (hw_info_intf_.empty()) { return kErrorUndefined; }
  // hw_info_intf_[0] is real DPU (core0), so primary display interface type is
  // determined by the real DPU connector, not virtual/SPI cores.
  return hw_info_intf_[0]->GetFirstDisplayInterfaceType(hw_disp_info);
}

DisplayError CoreImpl::GetDisplaysStatus(HWDisplaysInfo *hw_displays_info) {
  SCOPE_LOCK(locker_);
  hw_displays_info->clear();
  bool any_success = false;
  for (auto hw_info : hw_info_intf_) {
    HWDisplaysInfo display_infos;
    DisplayError error = hw_info->GetDisplaysStatus(&display_infos);
    if (error != kErrorNone) {
      DLOGW("GetDisplaysStatus failed for core %u. Error = %d", hw_info->GetCoreId(), error);
      continue;
    }
    any_success = true;
    hw_displays_info->insert(display_infos.begin(), display_infos.end());
  }
  // Needed for error-checking in CreateDisplay(int32_t display_id, ...) and getting display-type.
  hw_displays_info_ = *hw_displays_info;
  if (!any_success && !hw_info_intf_.empty()) {
    return kErrorUndefined;
  }
  return kErrorNone;
}

DisplayError CoreImpl::GetMaxDisplaysSupported(DisplayType type, int32_t *max_displays) {
  SCOPE_LOCK(locker_);
  *max_displays = 0;
  bool any_success = false;
  for (auto hw_info : hw_info_intf_) {
    int32_t tmp = 0;
    DisplayError error = hw_info->GetMaxDisplaysSupported(type, &tmp);
    if (error != kErrorNone) {
      // Degrade gracefully, consistent with HWInfoInterface::Create()/GetDisplaysStatus():
      // a per-core failure (e.g. card1/SPI absent) shouldn't fail the whole query when at
      // least one other core answered.
      DLOGW("GetMaxDisplaysSupported failed for core %u. Error = %d", hw_info->GetCoreId(),
            error);
      continue;
    }
    any_success = true;
    *max_displays += tmp;
  }
  if (!any_success && !hw_info_intf_.empty()) {
    return kErrorUndefined;
  }
  return kErrorNone;
}

bool CoreImpl::IsRotatorSupportedFormat(LayerBufferFormat format) {
  SCOPE_LOCK(locker_);
  return comp_mgr_.IsRotatorSupportedFormat(format);
}

}  // namespace sdm
