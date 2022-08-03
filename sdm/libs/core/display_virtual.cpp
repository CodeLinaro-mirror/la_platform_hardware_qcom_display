/*
* Copyright (c) 2014 - 2021, The Linux Foundation. All rights reserved.
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
* Changes from Qualcomm Innovation Center are provided under the following license:
* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
  SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <utils/constants.h>
#include <utils/debug.h>
#include <algorithm>
#include <vector>
#include "display_virtual.h"
#include "hw_interface.h"
#include "hw_info_interface.h"

#define __CLASS__ "DisplayVirtual"

namespace sdm {

DisplayVirtual::DisplayVirtual(DisplayEventHandler *event_handler,
                               std::vector<HWInfoInterface*> hw_info_intf,
                               BufferAllocator *buffer_allocator, CompManager *comp_manager)
  : DisplayBase(kVirtual, event_handler, kDeviceVirtual, buffer_allocator,
                comp_manager, hw_info_intf) {
}

DisplayVirtual::DisplayVirtual(DisplayId display_id, DisplayEventHandler *event_handler,
                               std::vector<HWInfoInterface*> hw_info_intf,
                               BufferAllocator *buffer_allocator, CompManager *comp_manager)
  : DisplayBase(display_id, kVirtual, event_handler, kDeviceVirtual,
                buffer_allocator, comp_manager, hw_info_intf) {
}

DisplayError DisplayVirtual::Init() {
  ClientLock lock(disp_mutex_);

  DisplayError error = kErrorNone;
  dpu_core_mux_ = new DPUCoreMux(display_id_info_, kVirtual, hw_info_intf_, buffer_allocator_);

  if (error != kErrorNone) {
    return error;
  }

  dpu_core_mux_->GetHWInterface(&hw_intf_);

  if (-1 == display_id_info_.GetDisplayId()) {
    dpu_core_mux_->GetDisplayId(&display_id_);
    display_id_info_ = DisplayId(primary_core_id_, display_id_);
    display_id_ = display_id_info_.GetDisplayId();
  }

  for (auto info_intf : hw_info_intf_) {
    HWResourceInfo hw_resource_info = HWResourceInfo();
    info_intf->GetHWResourceInfo(&hw_resource_info);
    hw_resource_info_.push_back(hw_resource_info);
  }

  uint32_t max_mixer_stages = INT_MAX;
  std::bitset<8> core_id_map = display_id_info_.GetCoreIdMap();
  for (int i = 0; i < core_id_map.size(); i++) {
    if (!core_id_map[i]) {
      continue;
    }

    max_mixer_stages = std::min(max_mixer_stages, hw_resource_info_[i].num_blending_stages);
  }

  int property_value = Debug::GetMaxPipesPerMixer(display_type_);
  if (property_value >= 0) {
    max_mixer_stages = std::min(UINT32(property_value), max_mixer_stages);
  }
  DisplayBase::SetMaxMixerStages(max_mixer_stages);

  int value = 0;
  Debug::Get()->GetProperty(DISABLE_MITIGATED_FPS, &value);
  disable_mitigated_fps_ = (value == 1);
  DLOGI("disable_mitigated_fps_: %d", disable_mitigated_fps_);

  value = 0;
  Debug::Get()->GetProperty(ENABLE_ASYNC_VDS_CREATION, &value);
  async_vds_creation_ = (value == 1);
  DLOGI("async_vds_creation: %d", async_vds_creation_);

  return error;
}

DisplayError DisplayVirtual::Deinit() {
  DisplayError error = DisplayBase::Deinit();
  float fps = 0;
  if (async_vds_creation_ && !disable_mitigated_fps_) {
    comp_manager_->GetConcurrencyFps(display_comp_ctx_,
                                     DisplayConcurrencyType::kConcurrencyWfd, &fps);
    if (fps != 0.0) {
      event_handler_->NotifyFpsMitigation(fps, DisplayConcurrencyType::kConcurrencyWfd, false);
    }
  }
  return error;
}

DisplayError DisplayVirtual::GetNumVariableInfoConfigs(uint32_t *count) {
  ClientLock lock(disp_mutex_);
  *count = 1;
  return kErrorNone;
}

DisplayError DisplayVirtual::GetConfig(uint32_t index, DisplayConfigVariableInfo *variable_info) {
  ClientLock lock(disp_mutex_);
  *variable_info = client_ctx_.display_attributes;
  return kErrorNone;
}

DisplayError DisplayVirtual::GetActiveConfig(uint32_t *index) {
  ClientLock lock(disp_mutex_);
  *index = 0;
  return kErrorNone;
}

DisplayError DisplayVirtual::SetActiveConfig(DisplayConfigVariableInfo *variable_info) {
  ClientLock lock(disp_mutex_);

  if (!variable_info) {
    return kErrorParameters;
  }

  DisplayError error = kErrorNone;
  DisplayClientContext client_ctx = {};
  DisplayDeviceContext device_ctx;
  client_ctx = client_ctx_;
  device_ctx = device_ctx_;

  client_ctx.display_attributes.x_pixels = variable_info->x_pixels;
  client_ctx.display_attributes.y_pixels = variable_info->y_pixels;
  client_ctx.display_attributes.fps = variable_info->fps;

  if (client_ctx.display_attributes == client_ctx_.display_attributes) {
    return kErrorNone;
  }

  error = dpu_core_mux_->SetDisplayAttributes(client_ctx.display_attributes);
  if (error != kErrorNone) {
    return error;
  }

  for (int i = 0; i < core_count_; i++) {
    device_ctx[i].display_attributes = client_ctx.display_attributes;
    device_ctx[i].display_attributes.x_pixels = client_ctx.display_attributes.x_pixels /
                                                core_count_;
  }

  dpu_core_mux_->GetHWPanelInfo(&device_ctx, &client_ctx);

  if (set_max_lum_ != -1.0 || set_min_lum_ != -1.0) {
    client_ctx.hw_panel_info.peak_luminance = set_max_lum_;
    client_ctx.hw_panel_info.blackness_level = set_min_lum_;
    DLOGI("set peak_luminance %f blackness_level %f", client_ctx.hw_panel_info.peak_luminance,
          client_ctx.hw_panel_info.blackness_level);
  }

  for (int i = 0; i < core_count_; i++) {
    device_ctx[i].hw_panel_info.peak_luminance = set_max_lum_;
    device_ctx[i].hw_panel_info.blackness_level = set_min_lum_;
  }

  error = dpu_core_mux_->GetMixerAttributes(&device_ctx, &client_ctx);
  if (error != kErrorNone) {
    return error;
  }

  // fb_config will be updated only once after creation of virtual display
  if (client_ctx.fb_config.x_pixels == 0 || client_ctx.fb_config.y_pixels == 0) {
    error = dpu_core_mux_->GetFbConfig(client_ctx.display_attributes.x_pixels,
                                       client_ctx.display_attributes.y_pixels,
                                       &device_ctx, &client_ctx);
      if (error != kErrorNone) {
        return error;
      }
  }

  // if display is already connected, reconfigure the display with new configuration.
  if (!display_comp_ctx_) {
    error = comp_manager_->RegisterDisplay(display_id_info_, display_type_, device_ctx, client_ctx,
                                           &display_comp_ctx_, &cached_qos_data_);
  } else {
    error = comp_manager_->ReconfigureDisplay(display_comp_ctx_, device_ctx, client_ctx,
                                              &cached_qos_data_);
  }
  if (error != kErrorNone) {
    return error;
  }

  if (async_vds_creation_ && !disable_mitigated_fps_) {
    float fps = 0;
    comp_manager_->GetConcurrencyFps(display_comp_ctx_,
                                     DisplayConcurrencyType::kConcurrencyWfd, &fps);
    if (fps != 0.0) {
      event_handler_->NotifyFpsMitigation(fps, DisplayConcurrencyType::kConcurrencyWfd, true);
    }
  }

  for (int i = 0; i < cached_qos_data_.size(); i++) {
    default_clock_hz_[i] = cached_qos_data_[i].clock_hz;
  }

  client_ctx_ = client_ctx;
  device_ctx_ = device_ctx;

  DLOGI("Virtual display resolution changed to[%dx%d]", client_ctx_.display_attributes.x_pixels,
        client_ctx_.display_attributes.y_pixels);

  return kErrorNone;
}

DisplayError DisplayVirtual::Prepare(LayerStack *layer_stack) {
  ClientLock lock(disp_mutex_);

  DisplayError error = PrePrepare(layer_stack);
  if (error == kErrorNone) {
    return error;
  }

  if (error == kErrorNeedsLutRegen && (ForceToneMapUpdate(layer_stack) == kErrorNone)) {
    return kErrorNone;
  }

  // Clean display layer stack for reuse.
  disp_layer_stack_ = DispLayerStack();
  disp_layer_stack_.info.resize(core_count_, {});
  return DisplayBase::Prepare(layer_stack);
}

DisplayError DisplayVirtual::GetColorModeCount(uint32_t *mode_count) {
  ClientLock lock(disp_mutex_);

  // Color Manager isn't supported for virtual displays.
  *mode_count = 1;

  return kErrorNone;
}

DisplayError DisplayVirtual::SetPanelLuminanceAttributes(float min_lum, float max_lum) {
  set_max_lum_ = max_lum;
  set_min_lum_ = min_lum;
  return kErrorNone;
}

DisplayError DisplayVirtual::colorSamplingOn() {
    return kErrorNone;
}

DisplayError DisplayVirtual::colorSamplingOff() {
    return kErrorNone;
}

}  // namespace sdm

