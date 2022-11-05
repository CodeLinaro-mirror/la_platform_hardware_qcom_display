/*
* Copyright (c) 2018-2019,2021 The Linux Foundation. All rights reserved.
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
*
* Changes from Qualcomm Innovation Center are provided under the following license:
*
* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*
*    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "hwc_display_dummy.h"
#include <utils/debug.h>

#define __CLASS__ "HWCDisplayDummy"

namespace sdm {

int HWCDisplayDummy::Create(CoreInterface *core_intf, BufferAllocator *buffer_allocator,
                            HWCCallbacks *callbacks, HWCDisplayEventHandler *event_handler,
                            qService::QService *qservice, hwc2_display_t id, int32_t sdm_id,
                            HWCDisplay **hwc_display) {
  return Create(core_intf, buffer_allocator, callbacks, event_handler,
                qservice, id, sdm_id, 3840, 2160, hwc_display); // Default resolution 3840x2160 (4K)
}

int HWCDisplayDummy::Create(CoreInterface *core_intf, BufferAllocator *buffer_allocator,
                            HWCCallbacks *callbacks, HWCDisplayEventHandler *event_handler,
                            qService::QService *qservice, hwc2_display_t id, int32_t sdm_id,
                            uint32_t primary_width, uint32_t primary_height,
                            HWCDisplay **hwc_display) {
  HWCDisplay *hwc_display_dummy = new HWCDisplayDummy(core_intf, buffer_allocator, callbacks,
                                                      event_handler, qservice, id, sdm_id,
                                                      primary_width, primary_height);
  hwc_display_dummy->SetValidationState(HWCDisplay::kSkipValidate);
  *hwc_display = hwc_display_dummy;
  return kErrorNone;
}

void HWCDisplayDummy::Destroy(HWCDisplay *hwc_display) {
  delete hwc_display;
}

HWC2::Error HWCDisplayDummy::Validate(uint32_t *out_num_types, uint32_t *out_num_requests) {
  for (auto hwc_layer : layer_set_) {
    if(hwc_layer->GetClientRequestedCompositionType() == HWC2::Composition::Cursor)
      layer_stack_.flags.cursor_present = true;
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayDummy::Present(int32_t *out_retire_fence) {
  for (auto hwc_layer : layer_set_) {
    hwc_layer->PushBackReleaseFence(-1);
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayDummy::SetColorMode(ColorMode mode) {
  return HWC2::Error::None;
}

HWCDisplayDummy::HWCDisplayDummy(CoreInterface *core_intf, BufferAllocator *buffer_allocator,
                                 HWCCallbacks *callbacks, HWCDisplayEventHandler *event_handler,
                                 qService::QService *qservice, hwc2_display_t id,
                                 int32_t sdm_id, uint32_t primary_width,
                                 uint32_t primary_height) :HWCDisplay(core_intf, buffer_allocator,
                                 callbacks, event_handler, qservice, kPluggable, id, sdm_id,
                                 DISPLAY_CLASS_PLUGGABLE) {
  DisplayConfigVariableInfo config;
  config.x_pixels = primary_width;
  config.y_pixels = primary_height;
  config.x_dpi = 300.0f;
  config.y_dpi = 300.0f;
  config.fps = 60;
  config.vsync_period_ns = 16600000;
  num_configs_ = 1;
  display_intf_ = &display_null_;
  client_target_ = new HWCLayer(id_, buffer_allocator_);
  current_refresh_rate_ = max_refresh_rate_ = 60;
  hwc_config_map_.resize(num_configs_);
  variable_config_map_[0] = config;
  hwc_config_map_.at(0) = 0;
  int status = SetFrameBufferResolution(config.x_pixels, config.y_pixels);
  if (status) {
    DLOGW("Set frame buffer config failed. Error = %d", status);
    return;
  }
}

HWC2::Error HWCDisplayDummy::GetActiveConfig(hwc2_config_t *out_config) {
  *out_config = 0;
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayDummy::UpdatePowerMode(HWC2::PowerMode mode) {
  current_power_mode_ = mode;
  return HWC2::Error::None;
}

}  // namespace sdm
