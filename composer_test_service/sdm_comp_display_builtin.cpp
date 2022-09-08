/*
* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#include <utils/constants.h>
#include <utils/debug.h>
#include <errno.h>
#include <unistd.h>

#include "sdm_comp_display_builtin.h"
#include "sdm_comp_debugger.h"
#include "utils/fence.h"
#include "utils/rect.h"


#define __CLASS__ "SDMCompDisplayBuiltIn"

namespace sdm {

SDMCompDisplayBuiltIn::SDMCompDisplayBuiltIn(CoreInterface *core_intf,
    SDMCompDisplayType disp_type, int32_t disp_id)
    : core_intf_(core_intf), display_type_(disp_type), display_id_(disp_id) {
}

int SDMCompDisplayBuiltIn::Init() {
  int status = 0;
  DisplayError error = core_intf_->CreateDisplay(display_id_, this, &display_intf_);
  if (error != kErrorNone) {
    if (kErrorDeviceRemoved == error) {
      DLOGW("Display creation cancelled. Display %d-%d removed.", display_id_, display_type_);
      return -ENODEV;
    } else {
      DLOGE("Display create failed. Error = %d display_id = %d event_handler = %p disp_intf = %p",
            error, display_id_, this, &display_intf_);
      return -EINVAL;
    }
  }

  error = display_intf_->SetDisplayState(kStateOn, false /* tear_down */, NULL /* release_fence */);
  if (error != kErrorNone) {
    DLOGE("Display power on failed, error = %d", error);
    status = -EINVAL;
    goto cleanup;
  }

  display_intf_->GetActiveConfig(&active_config_);
  display_intf_->GetConfig(active_config_, &variable_info_);
  display_intf_->SetCompositionState(kCompositionGPU, false);

  CreateLayerSet();
  return status;

cleanup:
  DestroyLayerSet();
  core_intf_->DestroyDisplay(display_intf_);
  return status;
}

int SDMCompDisplayBuiltIn::Deinit() {
  DisplayConfigFixedInfo fixed_info = {};
  display_intf_->GetConfig(&fixed_info);
  DisplayError error = kErrorNone;

  if (!fixed_info.is_cmdmode) {
    error = display_intf_->Flush(&layer_stack_);
    if (error != kErrorNone) {
      DLOGE("Flush failed. Error = %d", error);
      return -EINVAL;
    }
  }

  DestroyLayerSet();
  error = core_intf_->DestroyDisplay(display_intf_);
  if (error != kErrorNone) {
    DLOGE("Display destroy failed. Error = %d", error);
    return -EINVAL;
  }
  return 0;
}

int SDMCompDisplayBuiltIn::GetNumVariableInfoConfigs(uint32_t *count) {
  if (!count) {
    return -EINVAL;
  }
  DisplayError error = display_intf_->GetNumVariableInfoConfigs(count);
  if (error != kErrorNone) {
    return -EINVAL;
  }
  return 0;
}

int SDMCompDisplayBuiltIn::GetDisplayConfig(int config_idx,
                                            DisplayConfigVariableInfo *variable_info) {
  if (!variable_info) {
    return -EINVAL;
  }
  DisplayError error = display_intf_->GetConfig(config_idx, variable_info);
  if (error != kErrorNone) {
    return -EINVAL;
  }
  return 0;
}

int SDMCompDisplayBuiltIn::SetDisplayConfig(int config_idx) {
  DisplayError error = display_intf_->SetActiveConfig(config_idx);
  if (error != kErrorNone) {
    return -EINVAL;
  }
  active_config_ = config_idx;

  error = display_intf_->GetConfig(config_idx, &variable_info_);
  if (error != kErrorNone) {
    return -EINVAL;
  }
  return 0;
}

int SDMCompDisplayBuiltIn::GetDisplayAttributes(SDMCompDisplayAttributes *display_attributes) {
  if (!display_attributes) {
    return -EINVAL;
  }

  display_attributes->x_res = variable_info_.x_pixels;
  display_attributes->y_res = variable_info_.y_pixels;
  display_attributes->x_dpi = variable_info_.x_dpi;
  display_attributes->y_dpi = variable_info_.y_dpi;
  display_attributes->vsync_period = variable_info_.vsync_period_ns;
  display_attributes->is_yuv = variable_info_.is_yuv;
  display_attributes->fps = variable_info_.fps;
  display_attributes->smart_panel = variable_info_.smart_panel;

  return 0;
}

int SDMCompDisplayBuiltIn::ShowBuffer(BufferHandle *buf_handle, shared_ptr<Fence> *retire_fence) {
  layer_stack_.layers.clear();
  int status = PrepareLayerStack(buf_handle);
  if (status != 0) {
    DLOGE("PrepareLayerStack failed %d", status);
    return status;
  }

  if (qsync_mode_change_) {
    DLOGI("Setting Qsync Mode %d", qsync_mode_);
    display_intf_->SetQSyncMode(qsync_mode_);
    qsync_mode_change_ = false;
  }

  DisplayError error = display_intf_->CommitOrPrepare(&layer_stack_);
  if (error != kErrorNone && error != kErrorNeedsCommit) {
    DLOGW("CommitOrPrepare failed. Error = %d", error);
    return -EINVAL;
  }

  if (error == kErrorNeedsCommit) {
    DisplayError error = display_intf_->Commit(&layer_stack_);
    if (error != kErrorNone) {
      DLOGW("Commit failed. Error = %d", error);
      return -EINVAL;
    }
  }

  Layer *layer = layer_stack_.layers.at(0);
  *retire_fence = layer_stack_.retire_fence;
  buf_handle->consumer_fence = layer->input_buffer.release_fence;

  return 0;
}

void SDMCompDisplayBuiltIn::CreateLayerSet() {
  Layer *layer = new Layer();
  layer->flags.updating = 1;

  layer->src_rect = LayerRect(0, 0, variable_info_.x_pixels, variable_info_.y_pixels);
  layer->dst_rect = layer->src_rect;

  layer->input_buffer = {};
  layer->input_buffer.width = variable_info_.x_pixels;
  layer->input_buffer.height = variable_info_.y_pixels;
  layer->input_buffer.unaligned_width = variable_info_.x_pixels;
  layer->input_buffer.unaligned_height = variable_info_.y_pixels;
  layer->input_buffer.format = kFormatInvalid;
  layer->input_buffer.planes[0].fd = -1;
  layer->input_buffer.handle_id = -1;
  layer->frame_rate = variable_info_.fps;
  layer->blending = kBlendingPremultiplied;

  layer_set_.push_back(layer);
}

void SDMCompDisplayBuiltIn::DestroyLayerSet() {
  // Remove any layer if any and clear layer stack
  for (Layer *layer : layer_set_) {
    delete layer;
  }
  layer_set_.clear();
}

int SDMCompDisplayBuiltIn::PrepareLayerStack(BufferHandle *buf_handle) {
  if (!buf_handle) {
    DLOGE("buf_handle pointer is null");
    return -EINVAL;
  }
  Layer *layer = layer_set_.at(0);
  LayerRect src_crop = LayerRect(buf_handle->src_crop.left, buf_handle->src_crop.top,
                                 buf_handle->src_crop.right, buf_handle->src_crop.bottom);

  layer->input_buffer.width = buf_handle->aligned_width;
  layer->input_buffer.height = buf_handle->aligned_height;
  layer->input_buffer.unaligned_width = buf_handle->width;
  layer->input_buffer.unaligned_height = buf_handle->height;
  layer->input_buffer.format = buf_handle->format;
  layer->input_buffer.planes[0].fd = buf_handle->fd;
  layer->input_buffer.planes[0].stride = buf_handle->stride_in_bytes;
  layer->input_buffer.handle_id = buf_handle->buffer_id;
  layer->input_buffer.buffer_id = buf_handle->buffer_id;
  layer->frame_rate = variable_info_.fps;
  layer->blending = kBlendingPremultiplied;
  layer->src_rect = LayerRect(0, 0, buf_handle->width, buf_handle->height);
  if (IsValid(src_crop)) {
    layer->src_rect = Intersection(src_crop, layer->src_rect);
  }

  DLOGI("WxHxF %dx%dx%d Crop[LTRB] [%.0f %.0f %.0f %.0f] DstRect[LTRB] [%.0f %.0f %.0f %.0f]",
        layer->input_buffer.width, layer->input_buffer.height, layer->input_buffer.format,
        layer->src_rect.left, layer->src_rect.top, layer->src_rect.right, layer->src_rect.bottom,
        layer->dst_rect.left, layer->dst_rect.top, layer->dst_rect.right, layer->dst_rect.bottom);

  for (auto &it : layer_set_) {
    layer_stack_.layers.push_back(it);
  }

  return 0;
}

DisplayError SDMCompDisplayBuiltIn::HandleEvent(DisplayEvent event) {
  DLOGI("Received display event %d", event);
  return kErrorNone;
}

int SDMCompDisplayBuiltIn::SetPanelBrightness(float brightness_level) {
  if (brightness_level < min_panel_brightness_) {
    DLOGE("brightness level is invalid!! brightness_level %f, min_panel_brightness %f",
          brightness_level, min_panel_brightness_);
    return -EINVAL;
  }
  // if min_panel_brightness is not set, then set panel_brightness value as min_panel_brightness
  if (min_panel_brightness_ == 0.0f) {
    min_panel_brightness_ = brightness_level;
  }

  DisplayError err = display_intf_->SetPanelBrightness(brightness_level);
  if (err != kErrorNone) {
    return -EINVAL;
  }
  return 0;
}

int SDMCompDisplayBuiltIn::SetMinPanelBrightness(float min_brightness) {
  if (min_brightness < 0.0f || min_brightness > 1.0) {
    DLOGE("Invalid min brightness settings %f", min_brightness);
    return -EINVAL;
  }
  min_panel_brightness_ = min_brightness;
  return 0;
}

void SDMCompDisplayBuiltIn::SetQSyncMode(QSyncMode qsync_mode) {
  if (qsync_mode_ != qsync_mode) {
    qsync_mode_ = qsync_mode;
    qsync_mode_change_ = true;
  }
}

}  // namespace sdm
