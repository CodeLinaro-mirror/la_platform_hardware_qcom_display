/*
* Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
  SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <string>
#include <vector>

#include "dpu_core_mux.h"

#define __CLASS__ "DPUCoreMux"
#define zero_index 0

namespace sdm {

DPUCoreMux::DPUCoreMux(DisplayId display_id, DisplayType type,
                               std::vector<HWInfoInterface*> hw_info_intf,
                               BufferAllocator *buffer_allocator) : display_id_(display_id) {
  DisplayError error = kErrorNone;
  for (auto hw_info : hw_info_intf) {
    HWInterface *hw = nullptr;
    error = HWInterface::Create(display_id.GetConnId(), type, hw_info, buffer_allocator, &hw);
    if (error != kErrorNone) {
      DLOGE("HW interface create failed");
    }

    hw_intf_.push_back(hw);
  }
}

DisplayError DPUCoreMux::Destroy() {
  for (auto hw_intf : hw_intf_) {
    HWInterface::Destroy(hw_intf);
  }
  return kErrorNone;
}

DisplayError DPUCoreMux::GetDisplayId(int32_t *display_id) {
  return hw_intf_[0]->GetDisplayId(display_id);
}

DisplayError DPUCoreMux::GetActiveConfig(uint32_t *active_config) {
  std::vector<uint32_t> active_config_list;
  DisplayError error = kErrorNone;
  uint32_t active_config_val = 0;

  for (auto hw_intf : hw_intf_) {
    error = hw_intf->GetActiveConfig(&active_config_val);
    if (error != kErrorNone) {
      return error;
    }

    active_config_list.push_back(active_config_val);
  }

  if (!AreAllEntriesSame<uint32_t>(active_config_list)) {
    return kErrorUndefined;
  }
  *active_config = active_config_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetDefaultConfig(uint32_t *default_config) {
  std::vector<uint32_t> default_config_list;
  DisplayError error = kErrorNone;
  uint32_t default_config_val = 0;

  for (auto hw_intf : hw_intf_) {
    error = hw_intf->GetDefaultConfig(&default_config_val);
    if (error != kErrorNone) {
      return error;
    }

    default_config_list.push_back(default_config_val);
  }

  if (!AreAllEntriesSame<uint32_t>(default_config_list)) {
    return kErrorUndefined;
  }
  *default_config = default_config_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetNumDisplayAttributes(uint32_t *count) {
  std::vector<uint32_t> count_list;
  uint32_t count_val = 0;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetNumDisplayAttributes(&count_val);
    if (error != kErrorNone) {
      return error;
    }

    count_list.push_back(count_val);
  }

  if (!AreAllEntriesSame<uint32_t>(count_list)) {
    return kErrorUndefined;
  }
  *count = count_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetDisplayAttributes(uint32_t index,
                                                  HWDisplayAttributes *display_attributes) {
  std::vector<HWDisplayAttributes> display_attributes_list;
  HWDisplayAttributes display_attributes_val;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetDisplayAttributes(index, &display_attributes_val);
     if (error != kErrorNone) {
       return error;
     }

     display_attributes_list.push_back(display_attributes_val);
  }

  display_attributes_val = display_attributes_list[zero_index];
  for (int i = 1; i < display_attributes_list.size(); i++) {
    display_attributes_val.x_pixels += display_attributes_list[i].x_pixels;
    display_attributes_val.h_total += display_attributes_list[i].h_total;
  }

  *display_attributes = display_attributes_val;

  return kErrorNone;
}

DisplayError DPUCoreMux::GetHWPanelInfo(HWPanelInfo *panel_info) {
  std::vector<HWPanelInfo> panel_info_list;
  HWPanelInfo panel_info_val;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetHWPanelInfo(&panel_info_val);
     if (error != kErrorNone) {
       return error;
     }

     panel_info_list.push_back(panel_info_val);
  }

  if (!AreAllEntriesSame<HWPanelInfo>(panel_info_list)) {
    return kErrorUndefined;
  }
  *panel_info = panel_info_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::SetDisplayAttributes(uint32_t index) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetDisplayAttributes(index);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetDisplayAttributes(const HWDisplayAttributes &display_attributes) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetDisplayAttributes(display_attributes);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetConfigIndex(char *mode, uint32_t *index) {
  std::vector<uint32_t> index_list;
  uint32_t index_val;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetConfigIndex(mode, &index_val);
     if (error != kErrorNone) {
       return error;
     }

     index_list.push_back(index_val);
  }

  if (!AreAllEntriesSame<uint32_t>(index_list)) {
    return kErrorUndefined;
  }
  *index = index_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::PowerOn(std::vector<HWQosData> &qos_data, SyncPoints *sync_points) {
  std::vector<SyncPoints> sync_points_list;
  SyncPoints sync_points_val;
  DisplayError error = kErrorNone;

  for (int i = 0; i < hw_intf_.size(); i++) {
    error = hw_intf_[i]->PowerOn(qos_data[i], &sync_points_val);
    if (error != kErrorNone && error != kErrorDeferred) {
      return error;
    }
    sync_points_list.push_back(sync_points_val);
  }

  if (!sync_points_list.size()) {
    return kErrorUndefined;
  }

  // Null Check for the merge
  sync_points_val = sync_points_list[0];
  for (int i = 1; i < sync_points_list.size(); i++) {
    auto val = sync_points_list[i];
    sync_points_val.retire_fence = Fence::Merge(sync_points_val.retire_fence, val.retire_fence);
    sync_points_val.release_fence = Fence::Merge(sync_points_val.release_fence,
                                                 val.release_fence);
  }

  *sync_points = sync_points_val;
  return error;
}

DisplayError DPUCoreMux::PowerOff(bool teardown, SyncPoints *sync_points) {
  std::vector<SyncPoints> sync_points_list;
  SyncPoints sync_points_val;
  DisplayError error = kErrorNone;

  for (int i = 0; i < hw_intf_.size(); i++) {
    error = hw_intf_[i]->PowerOff(teardown, &sync_points_val);
    if (error != kErrorNone && error != kErrorDeferred) {
      return error;
    }
    sync_points_list.push_back(sync_points_val);
  }

  if (!sync_points_list.size()) {
    return kErrorUndefined;
  }

  sync_points_val = sync_points_list[0];
  for (int i = 1; i < sync_points_list.size(); i++) {
    auto val = sync_points_list[i];
    sync_points_val.retire_fence = Fence::Merge(sync_points_val.retire_fence, val.retire_fence);
    sync_points_val.release_fence = Fence::Merge(sync_points_val.release_fence,
                                                  val.release_fence);
  }

  *sync_points = sync_points_val;
  return error;
}

DisplayError DPUCoreMux::Doze(std::vector<HWQosData> &qos_data, SyncPoints *sync_points) {
  std::vector<SyncPoints> sync_points_list;
  SyncPoints sync_points_val;
  DisplayError error = kErrorNone;

  for (int i = 0; i < hw_intf_.size(); i++) {
    error = hw_intf_[i]->Doze(qos_data[i], &sync_points_val);
    if (error != kErrorNone && error != kErrorDeferred) {
      return error;
    }
    sync_points_list.push_back(sync_points_val);
  }

  sync_points_val = sync_points_list[0];
  for (int i = 1; i < sync_points_list.size(); i++) {
    auto val = sync_points_list[i];
    sync_points_val.retire_fence = Fence::Merge(sync_points_val.retire_fence, val.retire_fence);
    sync_points_val.release_fence = Fence::Merge(sync_points_val.release_fence,
                                                  val.release_fence);
  }

  *sync_points = sync_points_val;
  return error;
}

DisplayError DPUCoreMux::DozeSuspend(std::vector<HWQosData> &qos_data, SyncPoints *sync_points) {
  std::vector<SyncPoints> sync_points_list;
  SyncPoints sync_points_val;
  DisplayError error = kErrorNone;

  for (int i = 0; i < hw_intf_.size(); i++) {
    error = hw_intf_[i]->DozeSuspend(qos_data[i], &sync_points_val);
    if (error != kErrorNone && error != kErrorDeferred) {
      return error;
    }
    sync_points_list.push_back(sync_points_val);
  }

  sync_points_val = sync_points_list[0];
  for (int i = 1; i < sync_points_list.size(); i++) {
    auto val = sync_points_list[i];
    sync_points_val.retire_fence = Fence::Merge(sync_points_val.retire_fence, val.retire_fence);
    sync_points_val.release_fence = Fence::Merge(sync_points_val.release_fence,
                                                  val.release_fence);
  }

  *sync_points = sync_points_val;
  return error;
}

DisplayError DPUCoreMux::Standby(SyncPoints *sync_points) {
  std::vector<SyncPoints> sync_points_list;
  SyncPoints sync_points_val;
  DisplayError error = kErrorNone;

  for (int i = 0; i < hw_intf_.size(); i++) {
    error = hw_intf_[i]->Standby(&sync_points_val);
    if (error != kErrorNone && error != kErrorDeferred) {
      return error;
    }
    sync_points_list.push_back(sync_points_val);
  }

  sync_points_val = sync_points_list[0];
  for (int i = 1; i < sync_points_list.size(); i++) {
    auto val = sync_points_list[i];
    sync_points_val.retire_fence = Fence::Merge(sync_points_val.retire_fence, val.retire_fence);
    sync_points_val.release_fence = Fence::Merge(sync_points_val.release_fence,
                                                  val.release_fence);
  }

  *sync_points = sync_points_val;
  return error;
}

DisplayError DPUCoreMux::Validate(std::vector<HWLayersInfo> &hw_layers_info) {
  for (int i = 0; i < hw_intf_.size(); i++) {
    DisplayError error = hw_intf_[i]->Validate(&hw_layers_info[i]);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::Commit(std::vector<HWLayersInfo> &hw_layers_info) {
  for (int i = 0; i < hw_intf_.size(); i++) {
    DisplayError error = hw_intf_[i]->Commit(&hw_layers_info[i]);
    if (error != kErrorNone) {
      return error;
    }
  }

  shared_ptr<Fence> retire_fence = hw_layers_info[0].retire_fence;
  shared_ptr<Fence> sync_handle = hw_layers_info[0].sync_handle;
  shared_ptr<Fence> op_release_fence = hw_layers_info[0].output_buffer ?
                                       hw_layers_info[0].output_buffer->release_fence : nullptr;

#ifndef SDM_VIRTUAL_DRIVER
  if (!retire_fence || !sync_handle) {
    return kErrorUndefined;
  }
#endif

  for (int i = 1; i < hw_layers_info.size(); i++) {
    retire_fence = Fence::Merge(hw_layers_info[i].retire_fence, retire_fence);
    sync_handle = Fence::Merge(hw_layers_info[i].sync_handle, sync_handle);

    if (hw_layers_info[i].output_buffer) {
      op_release_fence = Fence::Merge(hw_layers_info[i].output_buffer->release_fence,
                                      op_release_fence);
    }
  }

  for (int i = 0; i < hw_layers_info.size(); i++) {
    if (!hw_layers_info[i].output_buffer) {
      continue;
    }

    hw_layers_info[i].output_buffer->release_fence = op_release_fence;
  }

  hw_layers_info[0].common_info->retire_fence = retire_fence;
  hw_layers_info[0].common_info->sync_handle = sync_handle;

  for (auto layers_info : hw_layers_info) {
    std::vector<Layer> &hw_layers = layers_info.hw_layers;
    for (auto &layer : hw_layers) {
      layer.input_buffer.release_fence = sync_handle;
    }
  }

  // To-Do: Revisit update_mask
  hw_layers_info[0].common_info->updates_mask = 0;
  return kErrorNone;
}

DisplayError DPUCoreMux::Flush(std::vector<HWLayersInfo> &hw_layers_info) {
  for (int i = 0; i < hw_intf_.size(); i++) {
    DisplayError error = hw_intf_[i]->Flush(&hw_layers_info[i]);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetPPFeaturesVersion(PPFeatureVersion *vers) {
  return hw_intf_[0]->GetPPFeaturesVersion(vers);
}

DisplayError DPUCoreMux::SetVSyncState(bool enable) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetVSyncState(enable);
     if (error != kErrorNone) {
       return error;
     }
  }
  return kErrorNone;
}

void DPUCoreMux::SetIdleTimeoutMs(uint32_t timeout_ms) {
  for (auto hw_intf : hw_intf_) {
     hw_intf->SetIdleTimeoutMs(timeout_ms);
  }
}

DisplayError DPUCoreMux::SetDisplayMode(const HWDisplayMode hw_display_mode) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetDisplayMode(hw_display_mode);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetRefreshRate(uint32_t refresh_rate) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetRefreshRate(refresh_rate);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetPanelBrightness(int level) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetPanelBrightness(level);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetHWScanInfo(HWScanInfo *scan_info) {
  std::vector<HWScanInfo> scan_info_list;
  HWScanInfo scan_info_val;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetHWScanInfo(&scan_info_val);
    if (error != kErrorNone) {
      return error;
    }

    scan_info_list.push_back(scan_info_val);
  }

  if (!AreAllEntriesSame<HWScanInfo>(scan_info_list)) {
    return kErrorUndefined;
  }
  *scan_info = scan_info_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetVideoFormat(uint32_t config_index, uint32_t *video_format) {
  std::vector<uint32_t> video_format_list;
  uint32_t video_format_val = 0;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetVideoFormat(config_index, &video_format_val);
    if (error != kErrorNone) {
      return error;
    }

    video_format_list.push_back(video_format_val);
  }

  if (!AreAllEntriesSame<uint32_t>(video_format_list)) {
    return kErrorUndefined;
  }
  *video_format = video_format_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetMaxCEAFormat(uint32_t *max_cea_format) {
  std::vector<uint32_t> max_cea_format_list;
  uint32_t max_cea_format_val = 0;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetMaxCEAFormat(&max_cea_format_val);
    if (error != kErrorNone) {
      return error;
    }

    max_cea_format_list.push_back(max_cea_format_val);
  }

  if (!AreAllEntriesSame<uint32_t>(max_cea_format_list)) {
    return kErrorUndefined;
  }
  *max_cea_format = max_cea_format_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::SetCursorPosition(std::vector<HWLayersInfo> &hw_layers_info,
                                               int x, int y) {
  for (int i = 0; i < hw_intf_.size(); i++) {
    DisplayError error = hw_intf_[i]->SetCursorPosition(&hw_layers_info[i], x, y);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::OnMinHdcpEncryptionLevelChange(uint32_t min_enc_level) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->OnMinHdcpEncryptionLevelChange(min_enc_level);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetPanelBrightness(int *level) {
  std::vector<int> level_list;
  int level_val = 0;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetPanelBrightness(&level_val);
    if (error != kErrorNone) {
      return error;
    }

    level_list.push_back(level_val);
  }

  if (!AreAllEntriesSame<int>(level_list)) {
    return kErrorUndefined;
  }
  *level = level_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::SetAutoRefresh(bool enable) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetAutoRefresh(enable);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetScaleLutConfig(HWScaleLutInfo *lut_info) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetScaleLutConfig(lut_info);
    if (error != kErrorNone) {
      return error;
    }
  }
  return kErrorNone;
}

DisplayError DPUCoreMux::UnsetScaleLutConfig() {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->UnsetScaleLutConfig();
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetMixerAttributes(const HWMixerAttributes &mixer_attributes) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetMixerAttributes(mixer_attributes);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetMixerAttributes(HWMixerAttributes *mixer_attributes) {
  std::vector<HWMixerAttributes> mixer_attributes_list;
  HWMixerAttributes mixer_attributes_val;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetMixerAttributes(&mixer_attributes_val);
     if (error != kErrorNone) {
       return error;
     }

     mixer_attributes_list.push_back(mixer_attributes_val);
  }

  mixer_attributes_val = mixer_attributes_list[0];
  for (int i = 1; i <  mixer_attributes_list.size(); i++) {
    mixer_attributes_val.width += mixer_attributes_list[i].width;
  }

  *mixer_attributes = mixer_attributes_val;

  return kErrorNone;
}

DisplayError DPUCoreMux::DumpDebugData() {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->DumpDebugData();
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetDppsFeature(void *payload, size_t size) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetDppsFeature(payload, size);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetPPConfig(void *payload, size_t size) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetPPConfig(payload, size);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetDppsFeatureInfo(void *payload, size_t size) {
  return hw_intf_[0]->GetDppsFeatureInfo(payload, size);
}

DisplayError DPUCoreMux::HandleSecureEvent(SecureEvent secure_event,
                                               std::vector<HWQosData> &qos_data) {
  for (int i = 0; i < hw_intf_.size(); i++) {
    DisplayError error = hw_intf_[i]->HandleSecureEvent(secure_event, qos_data[i]);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::ControlIdlePowerCollapse(bool enable, bool synchronous) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->ControlIdlePowerCollapse(enable, synchronous);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetDisplayDppsAdROI(void *payload) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetDisplayDppsAdROI(payload);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetDynamicDSIClock(uint64_t bit_clk_rate) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetDynamicDSIClock(bit_clk_rate);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetDynamicDSIClock(uint64_t *bit_clk_rate) {
  std::vector<uint64_t> bit_clk_rate_list;
  uint64_t bit_clk_rate_val = 0;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetDynamicDSIClock(&bit_clk_rate_val);
     if (error != kErrorNone) {
       return error;
     }

     bit_clk_rate_list.push_back(bit_clk_rate_val);
  }

  if (!AreAllEntriesSame<uint64_t>(bit_clk_rate_list)) {
    return kErrorUndefined;
  }
  *bit_clk_rate = bit_clk_rate_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetDisplayIdentificationData(uint8_t *out_port, uint32_t *out_data_size,
                                                          uint8_t *out_data) {
  return hw_intf_[zero_index]->GetDisplayIdentificationData(out_port, out_data_size, out_data);
}

DisplayError DPUCoreMux::SetFrameTrigger(FrameTriggerMode mode) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetFrameTrigger(mode);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::SetBLScale(uint32_t level) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetBLScale(level);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetPanelBlMaxLvl(uint32_t *max_bl) {
  std::vector<uint32_t> max_bl_list;
  uint32_t max_bl_val = 0;

  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->GetPanelBlMaxLvl(&max_bl_val);
     if (error != kErrorNone) {
       return error;
     }

     max_bl_list.push_back(max_bl_val);
  }

  if (!AreAllEntriesSame<uint32_t>(max_bl_list)) {
    return kErrorUndefined;
  }
  *max_bl = max_bl_list[0];

  return kErrorNone;
}

DisplayError DPUCoreMux::GetPanelBrightnessBasePath(std::string *base_path) const {
  return hw_intf_[zero_index]->GetPanelBrightnessBasePath(base_path);
}

DisplayError DPUCoreMux::SetBlendSpace(const PrimariesTransfer &blend_space) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->SetBlendSpace(blend_space);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::EnableSelfRefresh(SelfRefreshState self_refresh_state) {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->EnableSelfRefresh(self_refresh_state);
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetFeatureSupportStatus(const HWFeature feature, uint32_t *status) {
  std::vector<uint32_t> status_list;
  uint32_t status_val;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetFeatureSupportStatus(feature, &status_val);
    if (error != kErrorNone) {
      return error;
    }

    status_list.push_back(status_val);
  }

  if (!AreAllEntriesSame<uint32_t>(status_list)) {
    return kErrorUndefined;
  }
  *status = status_list[zero_index];

  return kErrorNone;
}

void DPUCoreMux::FlushConcurrentWriteback() {
  for (auto hw_intf : hw_intf_) {
     hw_intf->FlushConcurrentWriteback();
  }
}

DisplayError DPUCoreMux::SetAlternateDisplayConfig(uint32_t *alt_config) {
  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->SetAlternateDisplayConfig(alt_config);
    if (error != kErrorNone) {
      return error;
    }
  }

  return kErrorNone;
}

DisplayError DPUCoreMux::GetQsyncFps(uint32_t *qsync_fps) {
  std::vector<uint32_t> qsync_fps_list;
  uint32_t qsync_fps_val;

  for (auto hw_intf : hw_intf_) {
    DisplayError error = hw_intf->GetQsyncFps(&qsync_fps_val);
    if (error != kErrorNone) {
      return error;
    }

    qsync_fps_list.push_back(qsync_fps_val);
  }

  if (!AreAllEntriesSame<uint32_t>(qsync_fps_list)) {
    return kErrorUndefined;
  }
  *qsync_fps = qsync_fps_list[zero_index];

  return kErrorNone;
}

DisplayError DPUCoreMux::CancelDeferredPowerMode() {
  for (auto hw_intf : hw_intf_) {
     DisplayError error = hw_intf->CancelDeferredPowerMode();
     if (error != kErrorNone) {
       return error;
     }
  }

  return kErrorNone;
}

PanelFeaturePropertyIntf* DPUCoreMux::GetPanelFeaturePropertyIntf() {
  return hw_intf_[zero_index]->GetPanelFeaturePropertyIntf();
}

void DPUCoreMux::GetHWInterface(HWInterface **intf) {
  *intf = hw_intf_[zero_index];
}

template <typename T>
bool DPUCoreMux::AreAllEntriesSame(std::vector<T>& entries) {
  if (!entries.size()) {
    return true;
  }

  T val = entries[0];
  for (int i = 1; i < entries.size(); i++) {
    if (val != entries[i]) {
      return false;
    }
  }

  return true;
}

void DPUCoreMux::GetDRMDisplayToken(sde_drm::DRMDisplayToken *token) const {
  hw_intf_[zero_index]->GetDRMDisplayToken(token);
}

bool DPUCoreMux::IsPrimaryDisplay() const {
  bool is_primary_display = true;
  for (auto hw_intf : hw_intf_) {
    is_primary_display &= hw_intf->IsPrimaryDisplay();
  }

  return is_primary_display;
}

}  // namespace sdm
