/*
Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Changes from Qualcomm Innovation Center are provided under the following license:
Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <fcntl.h>
#include <utils/debug.h>
#include <utils/sys.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#include "hw_peripheral_drm.h"

#define __CLASS__ "HWPeripheralDRM"

using sde_drm::DRMDisplayType;
using sde_drm::DRMOps;
using sde_drm::DRMPowerMode;
using sde_drm::DppsFeaturePayload;
using sde_drm::DRMDppsFeatureInfo;
using sde_drm::DRMSecureMode;
using sde_drm::DRMCWbCaptureMode;

namespace sdm {

HWPeripheralDRM::HWPeripheralDRM(int32_t display_id, BufferSyncHandler *buffer_sync_handler,
                                 BufferAllocator *buffer_allocator, HWInfoInterface *hw_info_intf)
  : HWDeviceDRM(buffer_sync_handler, buffer_allocator, hw_info_intf) {
  disp_type_ = DRMDisplayType::PERIPHERAL;
  device_name_ = "Peripheral";
  display_id_ = display_id;
}

DisplayError HWPeripheralDRM::Init() {
  DisplayError ret = HWDeviceDRM::Init();
  if (ret != kErrorNone) {
    DLOGE("Init failed for %s", device_name_);
    return ret;
  }

  scalar_data_.resize(hw_resource_.hw_dest_scalar_info.count);
  dest_scalar_cache_.resize(hw_resource_.hw_dest_scalar_info.count);
  PopulateBitClkRates();

  topology_control_ = UINT32(sde_drm::DRMTopologyControl::DSPP);
  if (hw_panel_info_.is_primary_panel) {
    topology_control_ |= UINT32(sde_drm::DRMTopologyControl::DEST_SCALER);
  }

  return kErrorNone;
}

void HWPeripheralDRM::PopulateBitClkRates() {
  if (!hw_panel_info_.dyn_bitclk_support) {
    return;
  }

  // Group all bit_clk_rates corresponding to DRM_PREFERRED mode.
  uint32_t width = connector_info_.modes[current_mode_index_].mode.hdisplay;
  uint32_t height = connector_info_.modes[current_mode_index_].mode.vdisplay;

  for (auto &mode_info : connector_info_.modes) {
    auto &mode = mode_info.mode;
    if (mode.hdisplay == width && mode.vdisplay == height) {
      if (std::find(bitclk_rates_.begin(), bitclk_rates_.end(), mode_info.bit_clk_rate) ==
            bitclk_rates_.end()) {
        bitclk_rates_.push_back(mode_info.bit_clk_rate);
        DLOGI("Possible bit_clk_rates %d", mode_info.bit_clk_rate);
      }
    }
  }

  hw_panel_info_.bitclk_rates = bitclk_rates_;
  DLOGI("bit_clk_rates Size %d", bitclk_rates_.size());
}

DisplayError HWPeripheralDRM::SetDynamicDSIClock(uint64_t bit_clk_rate) {
  if (last_power_mode_ == DRMPowerMode::DOZE_SUSPEND || last_power_mode_ == DRMPowerMode::OFF) {
    return kErrorNotSupported;
  }

  bit_clk_rate_ = bit_clk_rate;
  update_mode_ = true;

  return kErrorNone;
}

DisplayError HWPeripheralDRM::GetDynamicDSIClock(uint64_t *bit_clk_rate) {
  // Update bit_rate corresponding to current refresh rate.
  *bit_clk_rate = (uint32_t)connector_info_.modes[current_mode_index_].bit_clk_rate;

  return kErrorNone;
}

DisplayError HWPeripheralDRM::Validate(HWLayers *hw_layers) {
  HWLayersInfo &hw_layer_info = hw_layers->info;
  SetDestScalarData(hw_layer_info, true);
  SetIdlePCState();

  return HWDeviceDRM::Validate(hw_layers);
}

DisplayError HWPeripheralDRM::Commit(HWLayers *hw_layers) {
  HWLayersInfo &hw_layer_info = hw_layers->info;
  SetDestScalarData(hw_layer_info, false);
  SetupConcurrentWriteback(hw_layer_info, false);
  SetIdlePCState();

  DisplayError error = HWDeviceDRM::Commit(hw_layers);
  if (error != kErrorNone) {
    return error;
  }

  PostCommitConcurrentWriteback(hw_layer_info.stack->output_buffer);

  // Initialize to default after successful commit
  synchronous_commit_ = false;

  return error;
}

void HWPeripheralDRM::ResetDisplayParams() {
  sde_dest_scalar_data_ = {};
  for (uint32_t j = 0; j < scalar_data_.size(); j++) {
    scalar_data_[j] = {};
    dest_scalar_cache_[j] = {};
  }
}

void HWPeripheralDRM::SetDestScalarData(HWLayersInfo hw_layer_info, bool validate) {
  if (!hw_scale_ || !hw_resource_.hw_dest_scalar_info.count) {
    return;
  }

  for (uint32_t i = 0; i < hw_resource_.hw_dest_scalar_info.count && validate; i++) {
    DestScaleInfoMap::iterator it = hw_layer_info.dest_scale_info_map.find(i);

    if (it == hw_layer_info.dest_scale_info_map.end()) {
      continue;
    }

    HWDestScaleInfo *dest_scale_info = it->second;
    SDEScaler *scale = &scalar_data_[i];
    hw_scale_->SetScaler(dest_scale_info->scale_data, scale);

    sde_drm_dest_scaler_cfg *dest_scalar_data = &sde_dest_scalar_data_.ds_cfg[i];
    dest_scalar_data->flags = 0;
    if (scale->scaler_v2.enable) {
      dest_scalar_data->flags |= SDE_DRM_DESTSCALER_ENABLE;
    }
    if (scale->scaler_v2.de.enable) {
      dest_scalar_data->flags |= SDE_DRM_DESTSCALER_ENHANCER_UPDATE;
    }
    if (dest_scale_info->scale_update) {
      dest_scalar_data->flags |= SDE_DRM_DESTSCALER_SCALE_UPDATE;
    }
    if (hw_panel_info_.partial_update) {
      dest_scalar_data->flags |= SDE_DRM_DESTSCALER_PU_ENABLE;
    }
    dest_scalar_data->index = i;
    dest_scalar_data->lm_width = dest_scale_info->mixer_width;
    dest_scalar_data->lm_height = dest_scale_info->mixer_height;
    dest_scalar_data->scaler_cfg = reinterpret_cast<uint64_t>(&scale->scaler_v2);

    if (std::memcmp(&dest_scalar_cache_[i].scalar_data, scale, sizeof(SDEScaler)) ||
        dest_scalar_cache_[i].flags != dest_scalar_data->flags) {
      needs_ds_update_ = true;
    }
  }

  if (needs_ds_update_) {
    if (!validate) {
      // Cache the destination scalar data during commit
      for (uint32_t i = 0; i < hw_resource_.hw_dest_scalar_info.count; i++) {
        DestScaleInfoMap::iterator it = hw_layer_info.dest_scale_info_map.find(i);
        if (it == hw_layer_info.dest_scale_info_map.end()) {
          continue;
        }
        dest_scalar_cache_[i].flags = sde_dest_scalar_data_.ds_cfg[i].flags;
        dest_scalar_cache_[i].scalar_data = scalar_data_[i];
      }
      needs_ds_update_ = false;
    }
    sde_dest_scalar_data_.num_dest_scaler = UINT32(hw_layer_info.dest_scale_info_map.size());
    drm_atomic_intf_->Perform(DRMOps::CRTC_SET_DEST_SCALER_CONFIG, token_.crtc_id,
                              reinterpret_cast<uint64_t>(&sde_dest_scalar_data_));
  }
}

DisplayError HWPeripheralDRM::Flush(HWLayers *hw_layers) {
  DisplayError err = HWDeviceDRM::Flush(hw_layers);
  if (err != kErrorNone) {
    return err;
  }

  ResetDisplayParams();
  return kErrorNone;
}

DisplayError HWPeripheralDRM::SetDppsFeature(void *payload, size_t size) {
  uint32_t obj_id = 0, object_type = 0, feature_id = 0;
  uint64_t value = 0;

  if (size != sizeof(DppsFeaturePayload)) {
    DLOGE("invalid payload size %d, expected %d", size, sizeof(DppsFeaturePayload));
    return kErrorParameters;
  }

  DppsFeaturePayload *feature_payload = reinterpret_cast<DppsFeaturePayload *>(payload);
  object_type = feature_payload->object_type;
  feature_id = feature_payload->feature_id;
  value = feature_payload->value;

  if (feature_id == sde_drm::kFeatureAd4Roi) {
    if (feature_payload->value) {
      DisplayDppsAd4RoiCfg *params = reinterpret_cast<DisplayDppsAd4RoiCfg *>
                                                      (feature_payload->value);
      if (!params) {
        DLOGE("invalid playload value %d", feature_payload->value);
        return kErrorNotSupported;
      }

      ad4_roi_cfg_.h_x = params->h_start;
      ad4_roi_cfg_.h_y = params->h_end;
      ad4_roi_cfg_.v_x = params->v_start;
      ad4_roi_cfg_.v_y = params->v_end;
      ad4_roi_cfg_.factor_in = params->factor_in;
      ad4_roi_cfg_.factor_out = params->factor_out;

      value = (uint64_t)&ad4_roi_cfg_;
    }
  }

  if (object_type == DRM_MODE_OBJECT_CRTC) {
    obj_id = token_.crtc_id;
  } else if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
    obj_id = token_.conn_id;
  } else {
    DLOGE("invalid object type 0x%x", object_type);
    return kErrorUndefined;
  }

  drm_atomic_intf_->Perform(DRMOps::DPPS_CACHE_FEATURE, obj_id, feature_id, value);
  return kErrorNone;
}

DisplayError HWPeripheralDRM::GetDppsFeatureInfo(void *payload, size_t size) {
  if (size != sizeof(DRMDppsFeatureInfo)) {
    DLOGE("invalid payload size %d, expected %d", size, sizeof(DRMDppsFeatureInfo));
    return kErrorParameters;
  }
  DRMDppsFeatureInfo *feature_info = reinterpret_cast<DRMDppsFeatureInfo *>(payload);
  drm_mgr_intf_->GetDppsFeatureInfo(feature_info);
  return kErrorNone;
}

DisplayError HWPeripheralDRM::HandleSecureEvent(SecureEvent secure_event, HWLayers *hw_layers) {
  switch (secure_event) {
    case kSecureDisplayStart: {
      secure_display_active_ = true;
      if (hw_panel_info_.mode != kModeCommand) {
        DisplayError err = Flush(hw_layers);
        if (err != kErrorNone) {
          return err;
        }
      }
    }
    break;

    case kSecureDisplayEnd: {
      if (hw_panel_info_.mode != kModeCommand) {
        DisplayError err = Flush(hw_layers);
        if (err != kErrorNone) {
          return err;
        }
      }
      secure_display_active_ = false;
      synchronous_commit_ = true;
    }
    break;

    default:
      DLOGE("Invalid secure event %d", secure_event);
      return kErrorNotSupported;
  }

  return kErrorNone;
}

DisplayError HWPeripheralDRM::ControlIdlePowerCollapse(bool enable, bool synchronous) {
  sde_drm::DRMIdlePCState idle_pc_state =
    enable ? sde_drm::DRMIdlePCState::ENABLE : sde_drm::DRMIdlePCState::DISABLE;
  if (idle_pc_state == idle_pc_state_) {
    return kErrorNone;
  }
  // As idle PC is disabled after subsequent commit, Make sure to have synchrounous commit and
  // ensure TA accesses the display_cc registers after idle PC is disabled.
  idle_pc_state_ = idle_pc_state;
  synchronous_commit_ = !enable ? synchronous : false;
  return kErrorNone;
}

DisplayError HWPeripheralDRM::PowerOn(const HWQosData &qos_data, int *release_fence) {
  DTRACE_SCOPED();
  if (!drm_atomic_intf_) {
    DLOGE("DRM Atomic Interface is null!");
    return kErrorUndefined;
  }

  if (first_cycle_) {
    return kErrorNone;
  }
  drm_atomic_intf_->Perform(sde_drm::DRMOps::CRTC_SET_IDLE_PC_STATE, token_.crtc_id,
                            sde_drm::DRMIdlePCState::ENABLE);
  DisplayError err = HWDeviceDRM::PowerOn(qos_data, release_fence);
  if (err != kErrorNone) {
    return err;
  }
  idle_pc_state_ = sde_drm::DRMIdlePCState::ENABLE;

  return kErrorNone;
}

DisplayError HWPeripheralDRM::SetDisplayAttributes(uint32_t index) {
  HWDeviceDRM::SetDisplayAttributes(index);
  // update bit clk rates.
  hw_panel_info_.bitclk_rates = bitclk_rates_;

  return kErrorNone;
}

DisplayError HWPeripheralDRM::SetDisplayDppsAdROI(void *payload) {
  DisplayError err = kErrorNone;
  struct sde_drm::DppsFeaturePayload feature_payload = {};

  if (!payload) {
    DLOGE("Invalid payload parameter");
    return kErrorParameters;
  }

  feature_payload.object_type = DRM_MODE_OBJECT_CRTC;
  feature_payload.feature_id = sde_drm::kFeatureAd4Roi;
  feature_payload.value = (uint64_t)(payload);

  err = SetDppsFeature(&feature_payload, sizeof(feature_payload));
  if (err != kErrorNone) {
    DLOGE("Faid to SetDppsFeature feature_id = %d, err = %d",
           sde_drm::kFeatureAd4Roi, err);
  }

  return err;
}

DisplayError HWPeripheralDRM::SetPanelBrightness(int level) {
  char buffer[kMaxSysfsCommandLength] = {0};

  if (brightness_base_path_.empty()) {
    return kErrorHardware;
  }

  std::string brightness_node(brightness_base_path_ + "brightness");
  int fd = Sys::open_(brightness_node.c_str(), O_RDWR);
  if (fd < 0) {
    DLOGE("Failed to open node = %s, error = %s ", brightness_node.c_str(),
          strerror(errno));
    return kErrorFileDescriptor;
  }

  int32_t bytes = snprintf(buffer, kMaxSysfsCommandLength, "%d\n", level);
  ssize_t ret = Sys::pwrite_(fd, buffer, static_cast<size_t>(bytes), 0);
  if (ret <= 0) {
    DLOGE("Failed to write to node = %s, error = %s ", brightness_node.c_str(),
          strerror(errno));
    Sys::close_(fd);
    return kErrorHardware;
  }

  Sys::close_(fd);

  return kErrorNone;
}

DisplayError HWPeripheralDRM::GetPanelBrightness(int *level) {
  char value[kMaxStringLength] = {0};

  if (!level) {
    DLOGE("Invalid input, null pointer.");
    return kErrorParameters;
  }

  if (brightness_base_path_.empty()) {
    return kErrorHardware;
  }

  std::string brightness_node(brightness_base_path_ + "brightness");
  int fd = Sys::open_(brightness_node.c_str(), O_RDWR);
  if (fd < 0) {
    DLOGE("Failed to open brightness node = %s, error = %s", brightness_node.c_str(),
           strerror(errno));
    return kErrorFileDescriptor;
  }

  if (Sys::pread_(fd, value, sizeof(value), 0) > 0) {
    *level = atoi(value);
  } else {
    DLOGE("Failed to read panel brightness");
    Sys::close_(fd);
    return kErrorHardware;
  }

  Sys::close_(fd);

  return kErrorNone;
}

void HWPeripheralDRM::GetHWPanelMaxBrightness() {
  char value[kMaxStringLength] = {0};
  hw_panel_info_.panel_max_brightness = 255.0f;

  // Panel nodes, driver connector creation, and DSI probing all occur in sync, for each DSI. This
  // means that the connector_type_id - 1 will reflect the same # as the panel # for panel node.
  char s[kMaxStringLength] = {};
  snprintf(s, sizeof(s), "/sys/class/backlight/panel%d-backlight/",
           static_cast<int>(connector_info_.type_id - 1));
  brightness_base_path_.assign(s);

  std::string brightness_node(brightness_base_path_ + "max_brightness");
  int fd = Sys::open_(brightness_node.c_str(), O_RDONLY);
  if (fd < 0) {
    DLOGE("Failed to open max brightness node = %s, error = %s", brightness_node.c_str(),
          strerror(errno));
    return;
  }

  if (Sys::pread_(fd, value, sizeof(value), 0) > 0) {
    hw_panel_info_.panel_max_brightness = static_cast<float>(atof(value));
    DLOGI_IF(kTagDriverConfig, "Max brightness = %f", hw_panel_info_.panel_max_brightness);
  } else {
    DLOGE("Failed to read max brightness. error = %s", strerror(errno));
  }

  Sys::close_(fd);
  return;
}

}  // namespace sdm
