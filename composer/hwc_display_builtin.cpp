/*
* Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
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
 *  Changes from Qualcomm Innovation Center are provided under the following license:
 *
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cutils/properties.h>
#include <sync/sync.h>
#include <utils/constants.h>
#include <utils/debug.h>
#include <utils/utils.h>
#include <utils/rect.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <pthread.h>

#include <map>
#include <string>
#include <vector>

#include "hwc_display_builtin.h"
#include "hwc_display_virtual_dpu.h"
#include "hwc_debugger.h"
#include "hwc_session.h"

#define __CLASS__ "HWCDisplayBuiltIn"

namespace sdm {

static void SetRect(LayerRect &src_rect, GLRect *target) {
  target->left = src_rect.left;
  target->top = src_rect.top;
  target->right = src_rect.right;
  target->bottom = src_rect.bottom;
}

LayerRect GetFrameRect(bool first, uint32_t x_pixels, uint32_t y_pixels) {
  LayerRect primary_res = { 0, 0, FLOAT(x_pixels), FLOAT(y_pixels)};
  RectOrientation orientation = GetOrientation(primary_res);
  LayerRect rect = {};
  if (orientation == kOrientationPortrait) {
    if (first) {  // top
      rect.left = 0; rect.top = 0;
      rect.right = FLOAT(x_pixels); rect.bottom = FLOAT(y_pixels/2);
      Log(kTagClient, "FrameRect Top", rect);
    } else {  // bottom
      rect.left = 0; rect.top = FLOAT(y_pixels/2);
      rect.right = FLOAT(x_pixels); rect.bottom = FLOAT(y_pixels);
      Log(kTagClient, "FrameRect Bottom", rect);
    }
  } else if (orientation == kOrientationLandscape) {
    if (first) {  // Left
      rect.left = 0; rect.top = 0;
      rect.right = FLOAT(x_pixels/2); rect.bottom = FLOAT(y_pixels);
      Log(kTagClient, "FrameRect Left", rect);
    } else {  // right
      rect.left = FLOAT(x_pixels/2); rect.top = 0;
      rect.right = FLOAT(x_pixels); rect.bottom = FLOAT(y_pixels);
      Log(kTagClient, "FrameRect Right", rect);
    }
  }
  return rect;
}
int HWCDisplayBuiltIn::Create(CoreInterface *core_intf, BufferAllocator *buffer_allocator,
                              HWCCallbacks *callbacks, HWCDisplayEventHandler *event_handler,
                              qService::QService *qservice, hwc2_display_t id, int32_t sdm_id,
                              HWCDisplay **hwc_display) {
  int status = 0;
  uint32_t builtin_width = 0;
  uint32_t builtin_height = 0;

  HWCDisplay *hwc_display_builtin =
      new HWCDisplayBuiltIn(core_intf, buffer_allocator, callbacks, event_handler, qservice, id,
                            sdm_id);
  status = hwc_display_builtin->Init();
  if (status) {
    delete hwc_display_builtin;
    return status;
  }

  hwc_display_builtin->GetMixerResolution(&builtin_width, &builtin_height);
  int width = 0, height = 0;
  HWCDebugHandler::Get()->GetProperty(FB_WIDTH_PROP, &width);
  HWCDebugHandler::Get()->GetProperty(FB_HEIGHT_PROP, &height);
  if (width > 0 && height > 0) {
    builtin_width = UINT32(width);
    builtin_height = UINT32(height);
  }

  status = hwc_display_builtin->SetFrameBufferResolution(builtin_width, builtin_height);
  if (status) {
    Destroy(hwc_display_builtin);
    return status;
  }

  *hwc_display = hwc_display_builtin;

  return status;
}

void HWCDisplayBuiltIn::Destroy(HWCDisplay *hwc_display) {
  hwc_display->Deinit();
  delete hwc_display;
}

HWCDisplayBuiltIn::HWCDisplayBuiltIn(CoreInterface *core_intf, BufferAllocator *buffer_allocator,
                                     HWCCallbacks *callbacks, HWCDisplayEventHandler *event_handler,
                                     qService::QService *qservice, hwc2_display_t id,
                                     int32_t sdm_id)
    : HWCDisplay(core_intf, buffer_allocator, callbacks, event_handler, qservice, kBuiltIn, id,
                 sdm_id, DISPLAY_CLASS_BUILTIN),
      buffer_allocator_(buffer_allocator),
      cpu_hint_(NULL), layer_stitch_task_(*this) {
}

int HWCDisplayBuiltIn::Init() {
  cpu_hint_ = new CPUHint();
  if (cpu_hint_->Init(static_cast<HWCDebugHandler *>(HWCDebugHandler::Get())) != kErrorNone) {
    delete cpu_hint_;
    cpu_hint_ = NULL;
  }

  use_metadata_refresh_rate_ = true;
  int disable_metadata_dynfps = 0;
  HWCDebugHandler::Get()->GetProperty(DISABLE_METADATA_DYNAMIC_FPS_PROP, &disable_metadata_dynfps);
  if (disable_metadata_dynfps) {
    use_metadata_refresh_rate_ = false;
  }

  int status = HWCDisplay::Init();
  if (status) {
    return status;
  }
  color_mode_ = new HWCColorMode(display_intf_);
  color_mode_->Init();

  int value = 0;
  HWCDebugHandler::Get()->GetProperty(ENABLE_OPTIMIZE_REFRESH, &value);
  enable_optimize_refresh_ = (value == 1);
  if (enable_optimize_refresh_) {
    DLOGI("Drop redundant drawcycles %" PRIu64 , id_);
  }

  is_primary_ = display_intf_->IsPrimaryDisplay();

  if (is_primary_) {
    int enable_bw_limits = 0;
    HWCDebugHandler::Get()->GetProperty(ENABLE_BW_LIMITS, &enable_bw_limits);
    enable_bw_limits_ = (enable_bw_limits == 1);
    if (enable_bw_limits_) {
      DLOGI("Enable BW Limits %" PRIu64, id_);
    }
    windowed_display_ = Debug::GetWindowRect(&window_rect_.left, &window_rect_.top,
                      &window_rect_.right, &window_rect_.bottom) != kErrorUndefined;
    DLOGI("Window rect : [%f %f %f %f]", window_rect_.left, window_rect_.top,
          window_rect_.right, window_rect_.bottom);

    value = 0;
    HWCDebugHandler::Get()->GetProperty(ENABLE_POMS_DURING_DOZE, &value);
    enable_poms_during_doze_ = (value == 1);
    if (enable_poms_during_doze_) {
      DLOGI("Enable POMS during Doze mode %" PRIu64 , id_);
    }
  }

  uint32_t config_index = 0;
  GetActiveDisplayConfig(&config_index);
  DisplayConfigVariableInfo attr = {};
  GetDisplayAttributesForConfig(INT(config_index), &attr);
  active_refresh_rate_ = attr.fps;

  if (pthread_create(&wb_kickoff_thread_, NULL, &HWCDisplayBuiltIn::WBKickOffThread, this)) {
    DLOGI("Failed to start %s, error = %s", "WB kickoff thread", strerror(errno));
  }

  DLOGI("active_refresh_rate: %d", active_refresh_rate_);

  return status;
}

std::string HWCDisplayBuiltIn::Dump() {
  return HWCDisplay::Dump() + histogram.Dump();
}

void HWCDisplayBuiltIn::InitCacResources(uint32_t x_pixels, uint32_t y_pixels) {
  // Allocate o/p buffer and client target for WB
  int ret = AllocateWbBuffer(x_pixels, y_pixels, false);
  if (ret) {
    DLOGE("Error allocating WB buffers");
    return;
  }
  // allocate the sec layers for main builtin display
  // 1 layer + 1 GPU target and populate the LS
  HWCLayer *layer = *sec_layer_set_.emplace(new HWCLayer(id_,
    reinterpret_cast<HWCBufferAllocator *>(buffer_allocator_)));
  sec_layer_map_.emplace(std::make_pair(layer->GetId(), layer));
  wb_op_layer_ = layer;
  wb_op_layer_->SetLayerBuffer(wb_buffer_handle_, -1);
  hwc_rect_t rect = {0, 0, static_cast<int>(x_pixels), static_cast<int>(y_pixels) };
  wb_op_layer_->SetLayerDisplayFrame(rect);
  hwc_frect_t rect_f = { 0.0, 0.0, FLOAT(x_pixels), FLOAT(y_pixels) };
  wb_op_layer_->SetLayerSourceCrop(rect_f);


  // Dummy Client target(unused, no backing buffer_fd) on Primary
  new_client_target_ = new HWCLayer(id_,
     reinterpret_cast<HWCBufferAllocator *>(buffer_allocator_));

  Layer *sdm_layer = new_client_target_->GetSDMLayer();
  LayerRect pri_fb_rect = { 0, 0, FLOAT(x_pixels), FLOAT(y_pixels) };  // taken from primary
  sdm_layer->src_rect = pri_fb_rect;
  sdm_layer->dst_rect = pri_fb_rect;
  sdm_layer->input_buffer.unaligned_width = x_pixels;
  sdm_layer->input_buffer.unaligned_height = y_pixels;
  sdm_layer->input_buffer.width = x_pixels;   // aligned and unaligned are same, as its not used
  sdm_layer->input_buffer.height = x_pixels;  // aligned and unaligned are same, as its not used
}

int HWCDisplayBuiltIn::AllocateWbBuffer(uint32_t x_pixels, uint32_t y_pixels, bool secure) {
  wb_buffer_info_ = {};
  wb_buffer_info_.buffer_config.width = x_pixels;
  wb_buffer_info_.buffer_config.height = y_pixels;
  wb_buffer_info_.buffer_config.format = sdm::kFormatRGB888;
  wb_buffer_info_.buffer_config.cache = true;
  wb_buffer_info_.buffer_config.secure = secure;

  if (buffer_allocator_->AllocateBuffer(&wb_buffer_info_) != 0) {
    DLOGE("Error allocating WB out_put buffer");
    return -1;
  }

  wb_buffer_handle_ = static_cast<buffer_handle_t>(wb_buffer_info_.private_data);
  wb_pvt_handle_ = reinterpret_cast<private_handle_t *>(wb_buffer_info_.private_data);
  DLOGI("handle of o/p buff = %lu fd = %d", wb_pvt_handle_->id, wb_pvt_handle_->fd);
  return 0;
}

int HWCDisplayBuiltIn::ReAllocateWBOutputBuffer(bool secure) {
  if (wb_buffer_info_.buffer_config.secure != secure) {
    // Reallocate the the o/p buffer (same size) with secure attribute
    DLOGI_IF(kTagClient, "For display %d-%d allocating WB buffer with secure = %s", sdm_id_, type_,
             secure ? "true" : "false");
    buffer_allocator_->FreeBuffer(&wb_buffer_info_);
    wb_buffer_info_.buffer_config.secure = secure;
    if (buffer_allocator_->AllocateBuffer(&wb_buffer_info_) != 0) {
      DLOGE("WB Display: Reallocation failed");
      return -1;
    }

    wb_buffer_handle_ = static_cast<buffer_handle_t>(wb_buffer_info_.private_data);
    wb_pvt_handle_ = reinterpret_cast<private_handle_t *>(wb_buffer_info_.private_data);
    wb_op_layer_->SetLayerBuffer(wb_buffer_handle_, -1);
  }

  return 0;
}

bool HWCDisplayBuiltIn::SecureLayerPresent() {
  for (auto hwc_layer : layer_set_) {
    Layer *layer = hwc_layer->GetSDMLayer();
    if (layer->input_buffer.flags.secure) {
      return true;
    }
  }
  return false;
}

HWC2::Error HWCDisplayBuiltIn::ValidateAndCommitWB() {
  if (!enable_cac_) {
    return HWC2::Error::None;
  }
  // Sequence is for 0 deg panel.
  // Validate top/left
  // commit top/left
  // Validate bottom/right
  // Commit bottom/right
  int32_t rel_fence = -1;
  HWC2::Error err = HWC2::Error::None;

  DTRACE_SCOPED();

  // true -> top/left
  err = ValidateWB(true);
  if (err != HWC2::Error::None) {
    DLOGE("ValidateWB top/left failed");
    return err;
  }
  rel_fence = -1;
  err = PresentWB(true, &rel_fence);
  if (err == HWC2::Error::None) {
    CloseFd(&rel_fence);
  } else {
    DLOGE("Present top/left failed");
    return err;
  }

  // Bottom/right
  err = ValidateWB(false);
  if (err != HWC2::Error::None) {
    DLOGE("ValidateWB bottom/right failed");
    return err;
  }
  rel_fence = -1;
  err = PresentWB(false, &rel_fence);
  if (err == HWC2::Error::None) {
    CloseFd(&rel_fence);
  } else {
    DLOGE("Present bottom/right failed");
    return err;
  }

  return err;
}

HWC2::Error HWCDisplayBuiltIn::ValidateWB(bool first) {
  HWC2::Error err = HWC2::Error::None;
  HWCSession *hwc_session = HWCSession::GetInstance();
  HWCDisplayVirtualDPU *wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
                                     (hwc_session->GetDisplay(hwc_session->wb_display_));
  if (!wb_display) {
    DLOGE("Return no WB display for display = %lu", id_);
    return HWC2::Error::Unsupported;
  }

  DTRACE_SCOPED();
  uint32_t x_pixels, y_pixels = 0;
  GetMixerResolution(&x_pixels, &y_pixels);
  LayerRect frame_split_rect = GetFrameRect(first, x_pixels, y_pixels);
  wb_display->SetFrameSplitRect(frame_split_rect);
  HWCLayerStack pri_layer_stack;
  GetLayerStack(&pri_layer_stack);
  wb_display->SetLayerStack(&pri_layer_stack);
  bool secure = SecureLayerPresent();
  ReAllocateWBOutputBuffer(secure);
  err = wb_display->SetOutputBuffer(wb_buffer_handle_, -1);
  uint32_t out_num_types;
  uint32_t out_num_requests;
  err = wb_display->Validate(&out_num_types, &out_num_requests);
  if (err != HWC2::Error::None && err != HWC2::Error::HasChanges) {
    DLOGE("Failed err = %d", err);
    return err;
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::PresentWB(bool first, int32_t *out_wb_release) {
  HWC2::Error err = HWC2::Error::None;
  HWCSession *hwc_session = HWCSession::GetInstance();

  HWCDisplayVirtualDPU *wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
                                     (hwc_session->GetDisplay(hwc_session->wb_display_));
  if (!wb_display) {
    DLOGE("Return no WB display for display = %lu", id_);
    return HWC2::Error::Unsupported;
  }
  DTRACE_SCOPED();

  err = wb_display->Present(out_wb_release);
  if (err != HWC2::Error::None) {
    DLOGE("WB Present failed: err = %d", err);
    return err;
  }

  // DumpOutputWB(*out_wb_release);  // Uncomment to dump the WB o/p in CAC

  return HWC2::Error::None;
}

void HWCDisplayBuiltIn::DumpOutputWB(int32_t out_wb_release) {
  if (wb_pvt_handle_) {
    const private_handle_t *output_handle =
        reinterpret_cast<const private_handle_t *>(wb_pvt_handle_);
    DisplayError error = kErrorNone;
    HWCBufferAllocator *buffer_allocator =
        reinterpret_cast<HWCBufferAllocator *>(buffer_allocator_);
    if (!output_handle->base) {
      error = buffer_allocator->MapBuffer(wb_pvt_handle_, out_wb_release);
      if (error != kErrorNone) {
        DLOGE("Failed to map output buffer, error = %d", error);
        return;
      }
    }
    DumpOutputBuffer(wb_buffer_info_, reinterpret_cast<void *>(output_handle->base),
                     out_wb_release);

    int release_fence = -1;
    error = buffer_allocator->UnmapBuffer(output_handle, &release_fence);
    if (error != kErrorNone) {
      DLOGE("Failed to unmap buffer, error = %d", error);
      return;
    }
  } else {
    DLOGE("No WB Output Buffer to dump!!");
  }
}

void HWCDisplayBuiltIn::BuildLayerStackFromWB() {
  layer_stack_ = LayerStack();
  display_rect_ = LayerRect();
  metadata_refresh_rate_ = 0;
  layer_stack_.flags.animating = animating_;
  layer_stack_.flags.fast_path = fast_path_enabled_ && fast_path_composition_;

  DTRACE_SCOPED();
  // Add one layer for fb target
  for (auto hwc_layer : sec_layer_set_) {
    // Reset layer data which SDM may change
    hwc_layer->ResetPerFrameData();
    Layer *layer = hwc_layer->GetSDMLayer();
    layer->flags = {};   // Reset earlier flags
    // Mark all layers to skip, when client target handle is NULL
    if (hwc_layer->GetClientRequestedCompositionType() == HWC2::Composition::Client ||
        !client_target_->GetSDMLayer()->input_buffer.buffer_id) {
      layer->flags.skip = true;
    } else if (hwc_layer->GetClientRequestedCompositionType() == HWC2::Composition::SolidColor) {
      layer->flags.solid_fill = true;
    }

    if (!hwc_layer->IsDataSpaceSupported()) {
      layer->flags.skip = true;
    }

    // set default composition as GPU for SDM
    layer->composition = kCompositionGPU;

    if (swap_interval_zero_) {
      if (layer->input_buffer.acquire_fence_fd >= 0) {
        close(layer->input_buffer.acquire_fence_fd);
        layer->input_buffer.acquire_fence_fd = -1;
      }
    }

    bool is_secure = false;
    bool is_video = false;
    const private_handle_t *handle =
        reinterpret_cast<const private_handle_t *>(layer->input_buffer.buffer_id);
    if (handle) {
      if (handle->buffer_type == BUFFER_TYPE_VIDEO) {
        layer_stack_.flags.video_present = true;
        is_video = true;
      }
      // TZ Protected Buffer - L1
      // Gralloc Usage Protected Buffer - L3 - which needs to be treated as Secure & avoid fallback
      if (handle->flags & private_handle_t::PRIV_FLAGS_PROTECTED_BUFFER ||
          handle->flags & private_handle_t::PRIV_FLAGS_SECURE_BUFFER) {
        layer_stack_.flags.secure_present = true;
        is_secure = true;
      }
      // UBWC PI format
      if (handle->flags & private_handle_t::PRIV_FLAGS_UBWC_ALIGNED_PI) {
        layer->input_buffer.flags.ubwc_pi = true;
      }
    }
    if (layer->input_buffer.flags.secure_display) {
      layer_stack_.flags.secure_present = true;
      is_secure = true;
    }

    if (hwc_layer->IsSingleBuffered() &&
       !(hwc_layer->IsRotationPresent() || hwc_layer->IsScalingPresent())) {
      layer->flags.single_buffer = true;
      layer_stack_.flags.single_buffered_layer_present = true;
    }

    bool hdr_layer = layer->input_buffer.color_metadata.colorPrimaries == ColorPrimaries_BT2020 &&
                     (layer->input_buffer.color_metadata.transfer == Transfer_SMPTE_ST2084 ||
                     layer->input_buffer.color_metadata.transfer == Transfer_HLG);
    if (hdr_layer && !disable_hdr_handling_) {
      // Dont honor HDR when its handling is disabled
      layer->input_buffer.flags.hdr = true;
      layer_stack_.flags.hdr_present = true;
    }

    if (hwc_layer->IsNonIntegralSourceCrop() && !is_secure && !hdr_layer &&
        !layer->flags.single_buffer && !layer->flags.solid_fill && !is_video) {
      layer->flags.skip = true;
    }

    if (!layer->flags.skip &&
        (hwc_layer->GetClientRequestedCompositionType() == HWC2::Composition::Cursor)) {
      // Currently we support only one HWCursor & only at top most z-order
      if ((*sec_layer_set_.rbegin())->GetId() == hwc_layer->GetId()) {
        layer->flags.cursor = true;
        layer_stack_.flags.cursor_present = true;
      }
    }

    if (layer->flags.skip) {
      layer_stack_.flags.skip_present = true;
    }

    // TODO(user): Move to a getter if this is needed at other places
    hwc_rect_t scaled_display_frame = {INT(layer->dst_rect.left), INT(layer->dst_rect.top),
                                       INT(layer->dst_rect.right), INT(layer->dst_rect.bottom)};
    if (hwc_layer->GetGeometryChanges() & kDisplayFrame) {
      ApplyScanAdjustment(&scaled_display_frame);
    }
    hwc_layer->SetLayerDisplayFrame(scaled_display_frame);
    hwc_layer->ResetPerFrameData();
    // SDM requires these details even for solid fill
    if (layer->flags.solid_fill) {
      LayerBuffer *layer_buffer = &layer->input_buffer;
      layer_buffer->width = UINT32(layer->dst_rect.right - layer->dst_rect.left);
      layer_buffer->height = UINT32(layer->dst_rect.bottom - layer->dst_rect.top);
      layer_buffer->unaligned_width = layer_buffer->width;
      layer_buffer->unaligned_height = layer_buffer->height;
      layer_buffer->acquire_fence_fd = -1;
      layer_buffer->release_fence_fd = -1;
      layer->src_rect.left = 0;
      layer->src_rect.top = 0;
      layer->src_rect.right = layer_buffer->width;
      layer->src_rect.bottom = layer_buffer->height;
    }

    if (hwc_layer->HasMetaDataRefreshRate() && layer->frame_rate > metadata_refresh_rate_) {
      metadata_refresh_rate_ = SanitizeRefreshRate(layer->frame_rate);
    }

    display_rect_ = Union(display_rect_, layer->dst_rect);
    geometry_changes_ |= hwc_layer->GetGeometryChanges();

    layer->flags.updating = true;
    if (sec_layer_set_.size() <= kMaxLayerCount) {
      layer->flags.updating = IsLayerUpdating(hwc_layer);
    }

    if (hwc_layer->IsColorTransformSet()) {
      layer->flags.color_transform = true;
    }

    layer_stack_.flags.mask_present |= layer->input_buffer.flags.mask_layer;

    if ((hwc_layer->GetDeviceSelectedCompositionType() != HWC2::Composition::Device) ||
        (hwc_layer->GetClientRequestedCompositionType() != HWC2::Composition::Device) ||
        layer->flags.skip) {
      layer->update_mask.set(kClientCompRequest);
    }

    if (game_supported_ && (hwc_layer->GetType() == kLayerGame)) {
      layer->flags.is_game = true;
      layer->input_buffer.flags.game = true;
    }

    layer_stack_.layers.push_back(layer);
  }

  // If layer stack needs Client composition, HWC display gets into InternalValidate state. If
  // validation gets reset by any other thread in this state, enforce Geometry change to ensure
  // that Client target gets composed by SF.
  bool enforce_geometry_change = (validate_state_ == kInternalValidate) && !validated_;

  // TODO(user): Set correctly when SDM supports geometry_changes as bitmask

  layer_stack_.flags.geometry_changed = UINT32((geometry_changes_ || enforce_geometry_change ||
                                                geometry_changes_on_doze_suspend_) > 0);
  layer_stack_.flags.config_changed = !validated_;
  // Append client target to the layer stack
  Layer *wb_client_target = new_client_target_->GetSDMLayer();
  wb_client_target->composition = kCompositionGPUTarget;
  wb_client_target->flags.updating = IsLayerUpdating(client_target_);
  // Derive client target dataspace based on the color mode - bug/115482728
  new_client_target_->SetLayerDataspace(client_target_->GetLayerDataspace());
  layer_stack_.layers.push_back(wb_client_target);

  // Set split rect to LS - for completion
  layer_stack_.frame_split = frame_split_rect_;
#if 0
  // fall back frame composition to GPU when client target is 10bit
  // TODO(user): clarify the behaviour from Client(SF) and SDM Extn -
  // when handling 10bit FBT, as it would affect blending
  if (Is10BitFormat(sdm_client_target->input_buffer.format)) {
    // Must fall back to client composition
    MarkLayersForClientComposition();
  }
#endif
}

HWC2::Error HWCDisplayBuiltIn::Validate(uint32_t *out_num_types, uint32_t *out_num_requests) {
  DisplayError error = kErrorNone;
  auto status = HWC2::Error::None;

  DTRACE_SCOPED();

  bool is_wb_cac_in_use = IsWBCacInUse();
  if (enable_cac_ && is_wb_cac_in_use) {
    status = ValidateAndCommitWB();
    if (status != HWC2::Error::None) {
      DLOGE("ValidateWB failed");
      return status;
    }
  }

  if (display_paused_ || (!is_primary_ && active_secure_sessions_[kSecureDisplay])) {
    MarkLayersForGPUBypass();
    return status;
  }

  if (color_tranform_failed_) {
    // Must fall back to client composition
    MarkLayersForClientComposition();
  }

  // If WB is not used for CAC not need to BuildLS from WB
  if (enable_cac_ && is_wb_cac_in_use) {
    // Call new BuildLayerStack, which populates layers from secondary set
    BuildLayerStackFromWB();
  } else {
    BuildLayerStack();
  }

  // Add stitch layer to layer stack.
  AppendStitchLayer();

  // Checks and replaces layer stack for solid fill
  SolidFillPrepare();

  // Apply current Color Mode and Render Intent.
  if (color_mode_->ApplyCurrentColorModeWithRenderIntent(
      static_cast<bool>(layer_stack_.flags.hdr_present)) != HWC2::Error::None) {
    // Fallback to GPU Composition, if Color Mode can't be applied.
    MarkLayersForClientComposition();
  }

  // apply pending DE config
  PPPendingParams pending_action;
  PPDisplayAPIPayload req_payload;
  pending_action.action = kGetDetailedEnhancerData;
  pending_action.params = NULL;
  int err = display_intf_->ColorSVCRequestRoute(req_payload, NULL, &pending_action);
  if (!err && pending_action.action == kConfigureDetailedEnhancer) {
      err = SetHWDetailedEnhancerConfig(pending_action.params);
  }

  bool pending_output_dump = dump_frame_count_ && dump_output_to_file_;

  if (readback_buffer_queued_ || pending_output_dump) {
    CloseFd(&output_buffer_.release_fence_fd);
    // RHS values were set in FrameCaptureAsync() called from a binder thread. They are picked up
    // here in a subsequent draw round. Readback is not allowed for any secure use case.
    readback_configured_ = !layer_stack_.flags.secure_present;
    if (readback_configured_) {
      DisablePartialUpdateOneFrame();
      layer_stack_.output_buffer = &output_buffer_;
      layer_stack_.flags.post_processed_output = post_processed_output_;
    }
  }

  uint32_t num_updating_layers = GetUpdatingLayersCount();
  bool one_updating_layer = (num_updating_layers == 1);
  if (num_updating_layers != 0) {
    ToggleCPUHint(one_updating_layer);
  }

  uint32_t refresh_rate = GetOptimalRefreshRate(one_updating_layer);
  error = display_intf_->SetRefreshRate(refresh_rate, force_refresh_rate_);

  // Get the refresh rate set.
  display_intf_->GetRefreshRate(&refresh_rate);
  bool vsync_source = (callbacks_->GetVsyncSource() == id_);

  if (error == kErrorNone) {
    if (vsync_source && (current_refresh_rate_ < refresh_rate)) {
      DTRACE_BEGIN("HWC2::Vsync::Enable");
      // Display is ramping up from idle.
      // Client realizes need for resync upon change in config.
      // Since we know config has changed, triggering vsync proactively
      // can help in reducing pipeline delays to enable events.
      SetVsyncEnabled(HWC2::Vsync::Enable);
      DTRACE_END();
    }
    // On success, set current refresh rate to new refresh rate.
    current_refresh_rate_ = refresh_rate;
    if (enable_cac_ && is_wb_cac_in_use) {
      UpdateFramerateForCAC(current_refresh_rate_);
    }
  }

  if (layer_set_.empty()) {
    // Avoid flush for Command mode panel.
    flush_ = !client_connected_;
    validated_ = true;
    return status;
  }

  status = PrepareLayerStack(out_num_types, out_num_requests);
  pending_commit_ = true;
  return status;
}

HWC2::Error HWCDisplayBuiltIn::CommitLayerStack() {
  skip_commit_ = CanSkipCommit();
  return HWCDisplay::CommitLayerStack();
}

bool HWCDisplayBuiltIn::CanSkipCommit() {
  if (layer_stack_invalid_) {
    return false;
  }

  // Reject repeated drawcycle requests if it satisfies all conditions.
  // 1. None of the layerstack attributes changed.
  // 2. No new buffer latched.
  // 3. No refresh request triggered by HWC.
  // 4. This display is not source of vsync.
  bool buffers_latched = false;
  for (auto &hwc_layer : layer_set_) {
    buffers_latched |= hwc_layer->BufferLatched();
    hwc_layer->ResetBufferFlip();
  }

  bool vsync_source = (callbacks_->GetVsyncSource() == id_);
  bool skip_commit = enable_optimize_refresh_ && !pending_commit_ && !buffers_latched &&
                     !pending_refresh_ && !vsync_source;
  pending_refresh_ = false;

  return skip_commit;
}

HWC2::Error HWCDisplayBuiltIn::CommitStitchLayers() {
  if (disable_layer_stitch_) {
    return HWC2::Error::None;
  }

  if (!validated_ || skip_commit_) {
    return HWC2::Error::None;
  }

  LayerStitchContext ctx = {};
  Layer *stitch_layer = stitch_target_->GetSDMLayer();
  LayerBuffer &output_buffer = stitch_layer->input_buffer;
  for (auto &layer : layer_stack_.layers) {
    LayerComposition &composition = layer->composition;
    if (composition != kCompositionStitch) {
      continue;
    }

    StitchParams params = {};
    // Stitch target doesn't have an input fence.
    // Render all layers at specified destination.
    LayerBuffer &input_buffer = layer->input_buffer;
    params.src_hnd = reinterpret_cast<const private_handle_t *>(input_buffer.buffer_id);
    params.dst_hnd = reinterpret_cast<const private_handle_t *>(output_buffer.buffer_id);
    SetRect(layer->stitch_info.dst_rect, &params.dst_rect);
    SetRect(layer->stitch_info.slice_rect, &params.scissor_rect);
    params.src_acquire_fence_fd = input_buffer.acquire_fence_fd;

    ctx.stitch_params.push_back(params);
  }

  if (!ctx.stitch_params.size()) {
    // No layers marked for stitch.
    return HWC2::Error::None;
  }

  layer_stitch_task_.PerformTask(LayerStitchTaskCode::kCodeStitch, &ctx);
  // Set release fence.
  output_buffer.acquire_fence_fd = ctx.release_fence_fd;

  return HWC2::Error::None;
}

void HWCDisplayBuiltIn::CacheAvrStatus() {
  QSyncMode qsync_mode = kQSyncModeNone;

  DisplayError error = display_intf_->GetQSyncMode(&qsync_mode);
  if (error != kErrorNone) {
    return;
  }

  bool qsync_enabled = (qsync_mode != kQSyncModeNone);
  if (qsync_enabled_ != qsync_enabled) {
    qsync_reconfigured_ = true;
    qsync_enabled_ = qsync_enabled;
  } else {
    qsync_reconfigured_ = false;
  }
}

bool HWCDisplayBuiltIn::IsQsyncCallbackNeeded(bool *qsync_enabled, int32_t *refresh_rate,
                           int32_t *qsync_refresh_rate) {
  if (!qsync_reconfigured_) {
    return false;
  }

  bool vsync_source = (callbacks_->GetVsyncSource() == id_);
  // Qsync callback not needed if this display is not the source of vsync
  if (!vsync_source) {
    return false;
  }

  *qsync_enabled = qsync_enabled_;
  uint32_t current_rate = 0;
  display_intf_->GetRefreshRate(&current_rate);
  *refresh_rate = INT32(current_rate);
  *qsync_refresh_rate = min_refresh_rate_;

  return true;
}

int HWCDisplayBuiltIn::GetBwCode(const DisplayConfigVariableInfo &attr) {
  uint32_t min_refresh_rate = 0, max_refresh_rate = 0;
  display_intf_->GetRefreshRateRange(&min_refresh_rate, &max_refresh_rate);
  uint32_t fps = attr.smart_panel ? attr.fps : max_refresh_rate;

  if (fps <= 60) {
    return kBwLow;
  } else if (fps <= 90) {
    return kBwMedium;
  } else {
    return kBwHigh;
  }
}

void HWCDisplayBuiltIn::SetBwLimitHint(bool enable) {
  if (!enable_bw_limits_) {
    return;
  }

  if (!enable) {
    thermal_bandwidth_client_cancel_request(const_cast<char*>(kDisplayBwName));
    curr_refresh_rate_ = 0;
    return;
  }

  uint32_t config_index = 0;
  DisplayConfigVariableInfo attr = {};
  GetActiveDisplayConfig(&config_index);
  GetDisplayAttributesForConfig(INT(config_index), &attr);
  if (attr.fps != curr_refresh_rate_ || attr.smart_panel != is_smart_panel_) {
    int bw_code = GetBwCode(attr);
    int req_data = thermal_bandwidth_client_merge_input_info(bw_code, 0);
    int error = thermal_bandwidth_client_request(const_cast<char*>(kDisplayBwName), req_data);
    if (error) {
      DLOGE("Thermal bandwidth request failed %d", error);
    }
    curr_refresh_rate_ = attr.fps;
    is_smart_panel_ = attr.smart_panel;
  }
}

void HWCDisplayBuiltIn::SetPartialUpdate(DisplayConfigFixedInfo fixed_info) {
  partial_update_enabled_ = fixed_info.partial_update || (!fixed_info.is_cmdmode);
  for (auto hwc_layer : layer_set_) {
    hwc_layer->SetPartialUpdate(partial_update_enabled_);
  }
  client_target_->SetPartialUpdate(partial_update_enabled_);
}

HWC2::Error HWCDisplayBuiltIn::SetPowerMode(HWC2::PowerMode mode, bool teardown) {
  DisplayConfigFixedInfo fixed_info = {};
  display_intf_->GetConfig(&fixed_info);
  bool command_mode = fixed_info.is_cmdmode;

  auto status = HWCDisplay::SetPowerMode(mode, teardown);
  if (status != HWC2::Error::None) {
    return status;
  }

  display_intf_->GetConfig(&fixed_info);
  is_cmd_mode_ = fixed_info.is_cmdmode;
  if (is_cmd_mode_ != command_mode) {
    SetPartialUpdate(fixed_info);
  }

  if (mode == HWC2::PowerMode::Off) {
    SetBwLimitHint(false);
  }

  if (enable_cac_ && cac_commit_done_) {
    DLOGI("Resetting CAC Commit done.... mode = %d", mode);
    CacCommitDone(false);
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::Present(int32_t *out_retire_fence) {
  auto status = HWC2::Error::None;

  DTRACE_SCOPED();

  if (cac_commit_done_) {
    // No need to call present on primary for every frame
    return status;
  }

  if (!is_primary_ && active_secure_sessions_[kSecureDisplay]) {
    return status;
  } else if (display_paused_) {
    DisplayError error = display_intf_->Flush(&layer_stack_);
    validated_ = false;
    if (error != kErrorNone) {
      DLOGE("Flush failed. Error = %d", error);
    }
  } else {
    CacheAvrStatus();
    DisplayConfigFixedInfo fixed_info = {};
    display_intf_->GetConfig(&fixed_info);
    bool command_mode = fixed_info.is_cmdmode;

    status = CommitStitchLayers();
    if (status != HWC2::Error::None) {
      DLOGE("Stitch failed: %d", status);
      return status;
    }

    status = CommitLayerStack();
    if (status == HWC2::Error::None) {
      HandleFrameOutput();
      SolidFillCommit();
      PostCommitStitchLayers();
      status = HWCDisplay::PostCommitLayerStack(out_retire_fence);
      SetBwLimitHint(true);
      display_intf_->GetConfig(&fixed_info);
      is_cmd_mode_ = fixed_info.is_cmdmode;
      if (is_cmd_mode_ != command_mode) {
        SetPartialUpdate(fixed_info);
      }
    }
  }

  CloseFd(&output_buffer_.acquire_fence_fd);
  pending_commit_ = false;
  if (enable_cac_ && IsWBCacInUse()) {
    CacCommitDone(true);
  }
  return status;
}

void HWCDisplayBuiltIn::PostCommitStitchLayers() {
  if (disable_layer_stitch_) {
    return;
  }

  // Close Stitch buffer acquire fence.
  Layer *stitch_layer = stitch_target_->GetSDMLayer();
  LayerBuffer &output_buffer = stitch_layer->input_buffer;
  for (auto &layer : layer_stack_.layers) {
    LayerComposition &composition = layer->composition;
    if (composition != kCompositionStitch) {
      continue;
    }
    LayerBuffer &input_buffer = layer->input_buffer;
    input_buffer.release_fence_fd = Sys::dup_(output_buffer.acquire_fence_fd);
  }
  CloseFd(&output_buffer.acquire_fence_fd);
  CloseFd(&output_buffer.release_fence_fd);
}

HWC2::Error HWCDisplayBuiltIn::GetColorModes(uint32_t *out_num_modes, ColorMode *out_modes) {
  if (out_modes == nullptr) {
    *out_num_modes = color_mode_->GetColorModeCount();
  } else {
    color_mode_->GetColorModes(out_num_modes, out_modes);
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::GetRenderIntents(ColorMode mode, uint32_t *out_num_intents,
                                                RenderIntent *out_intents) {
  if (out_intents == nullptr) {
    *out_num_intents = color_mode_->GetRenderIntentCount(mode);
  } else {
    color_mode_->GetRenderIntents(mode, out_num_intents, out_intents);
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetColorMode(ColorMode mode) {
  return SetColorModeWithRenderIntent(mode, RenderIntent::COLORIMETRIC);
}

HWC2::Error HWCDisplayBuiltIn::SetColorModeWithRenderIntent(ColorMode mode, RenderIntent intent) {
  auto status = color_mode_->CacheColorModeWithRenderIntent(mode, intent);
  if (status != HWC2::Error::None) {
    DLOGE("failed for mode = %d intent = %d", mode, intent);
    return status;
  }
  callbacks_->Refresh(id_);
  validated_ = false;
  return status;
}

HWC2::Error HWCDisplayBuiltIn::SetColorModeById(int32_t color_mode_id) {
  auto status = color_mode_->SetColorModeById(color_mode_id);
  if (status != HWC2::Error::None) {
    DLOGE("failed for mode = %d", color_mode_id);
    return status;
  }

  callbacks_->Refresh(id_);
  validated_ = false;

  return status;
}

HWC2::Error HWCDisplayBuiltIn::SetColorModeFromClientApi(int32_t color_mode_id) {
  DisplayError error = kErrorNone;
  std::string mode_string;

  error = display_intf_->GetColorModeName(color_mode_id, &mode_string);
  if (error) {
    DLOGE("Failed to get mode name for mode %d", color_mode_id);
    return HWC2::Error::BadParameter;
  }

  auto status = color_mode_->SetColorModeFromClientApi(mode_string);
  if (status != HWC2::Error::None) {
    DLOGE("Failed to set mode = %d", color_mode_id);
    return status;
  }

  return status;
}

HWC2::Error HWCDisplayBuiltIn::RestoreColorTransform() {
  auto status = color_mode_->RestoreColorTransform();
  if (status != HWC2::Error::None) {
    DLOGE("failed to RestoreColorTransform");
    return status;
  }

  callbacks_->Refresh(id_);

  return status;
}

HWC2::Error HWCDisplayBuiltIn::SetColorTransform(const float *matrix,
                                                 android_color_transform_t hint) {
  if (!matrix) {
    return HWC2::Error::BadParameter;
  }

  auto status = color_mode_->SetColorTransform(matrix, hint);
  if (status != HWC2::Error::None) {
    DLOGE("failed for hint = %d", hint);
    color_tranform_failed_ = true;
    return status;
  }

  callbacks_->Refresh(id_);
  color_tranform_failed_ = false;
  validated_ = false;

  return status;
}

HWC2::Error HWCDisplayBuiltIn::SetReadbackBuffer(const native_handle_t *buffer,
                                                 int32_t acquire_fence, bool post_processed_output,
                                                 CWBClient client) {
  if (cwb_client_ != client && cwb_client_ != kCWBClientNone) {
    DLOGE("CWB is in use with client = %d", cwb_client_);
    return HWC2::Error::NoResources;
  }

  const private_handle_t *handle = reinterpret_cast<const private_handle_t *>(buffer);
  if (!handle || (handle->fd < 0)) {
    return HWC2::Error::BadParameter;
  }

  // Configure the output buffer as Readback buffer
  output_buffer_.width = UINT32(handle->width);
  output_buffer_.height = UINT32(handle->height);
  output_buffer_.unaligned_width = UINT32(handle->unaligned_width);
  output_buffer_.unaligned_height = UINT32(handle->unaligned_height);
  output_buffer_.format = HWCLayer::GetSDMFormat(handle->format, handle->flags);
  output_buffer_.planes[0].fd = handle->fd;
  output_buffer_.planes[0].stride = UINT32(handle->width);
  output_buffer_.acquire_fence_fd = dup(acquire_fence);
  output_buffer_.release_fence_fd = -1;
  output_buffer_.handle_id = handle->id;

  post_processed_output_ = post_processed_output;
  readback_buffer_queued_ = true;
  readback_configured_ = false;
  validated_ = false;
  cwb_client_ = client;

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::GetReadbackBufferFence(int32_t *release_fence) {
  auto status = HWC2::Error::None;

  if (readback_configured_ && (output_buffer_.release_fence_fd >= 0)) {
    *release_fence = output_buffer_.release_fence_fd;
  } else {
    status = HWC2::Error::Unsupported;
    *release_fence = -1;
  }

  post_processed_output_ = false;
  readback_buffer_queued_ = false;
  readback_configured_ = false;
  output_buffer_ = {};
  cwb_client_ = kCWBClientNone;

  return status;
}

DisplayError HWCDisplayBuiltIn::TeardownConcurrentWriteback(void) {
  DisplayError error = kErrorNotSupported;

  if (output_buffer_.release_fence_fd >= 0) {
    int32_t release_fence_fd = dup(output_buffer_.release_fence_fd);
    int ret = sync_wait(output_buffer_.release_fence_fd, 1000);
    if (ret < 0) {
      DLOGE("sync_wait error errno = %d, desc = %s", errno, strerror(errno));
    }

    ::close(release_fence_fd);
    if (ret)
      return kErrorResources;
  }

  if (display_intf_) {
    error = display_intf_->TeardownConcurrentWriteback();
  }

  return error;
}

HWC2::Error HWCDisplayBuiltIn::SetDisplayDppsAdROI(uint32_t h_start, uint32_t h_end,
                                                   uint32_t v_start, uint32_t v_end,
                                                   uint32_t factor_in, uint32_t factor_out) {
  DisplayError error = kErrorNone;
  DisplayDppsAd4RoiCfg dpps_ad4_roi_cfg = {};
  uint32_t panel_width = 0, panel_height = 0;
  constexpr uint16_t kMaxFactorVal = 0xffff;

  if (h_start >= h_end || v_start >= v_end || factor_in > kMaxFactorVal ||
      factor_out > kMaxFactorVal) {
    DLOGE("Invalid roi region = [%u, %u, %u, %u, %u, %u]",
           h_start, h_end, v_start, v_end, factor_in, factor_out);
    return HWC2::Error::BadParameter;
  }

  GetPanelResolution(&panel_width, &panel_height);

  if (h_start >= panel_width || h_end > panel_width ||
      v_start >= panel_height || v_end > panel_height) {
    DLOGE("Invalid roi region = [%u, %u, %u, %u], panel resolution = [%u, %u]",
           h_start, h_end, v_start, v_end, panel_width, panel_height);
    return HWC2::Error::BadParameter;
  }

  dpps_ad4_roi_cfg.h_start = h_start;
  dpps_ad4_roi_cfg.h_end = h_end;
  dpps_ad4_roi_cfg.v_start = v_start;
  dpps_ad4_roi_cfg.v_end = v_end;
  dpps_ad4_roi_cfg.factor_in = factor_in;
  dpps_ad4_roi_cfg.factor_out = factor_out;

  error = display_intf_->SetDisplayDppsAdROI(&dpps_ad4_roi_cfg);
  if (error)
    return HWC2::Error::BadConfig;

  callbacks_->Refresh(id_);

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetFrameTriggerMode(uint32_t mode) {
  DisplayError error = kErrorNone;
  FrameTriggerMode trigger_mode = kFrameTriggerDefault;

  if (mode >= kFrameTriggerMax) {
    DLOGE("Invalid input mode %d", mode);
    return HWC2::Error::BadParameter;
  }

  trigger_mode = static_cast<FrameTriggerMode>(mode);
  error = display_intf_->SetFrameTriggerMode(trigger_mode);
  if (error)
    return HWC2::Error::BadConfig;

  callbacks_->Refresh(HWC_DISPLAY_PRIMARY);
  validated_ = false;

  return HWC2::Error::None;
}

int HWCDisplayBuiltIn::Perform(uint32_t operation, ...) {
  va_list args;
  va_start(args, operation);
  int val = 0;
  LayerSolidFill *solid_fill_color;
  LayerRect *rect = NULL;

  switch (operation) {
    case SET_METADATA_DYN_REFRESH_RATE:
      val = va_arg(args, int32_t);
      SetMetaDataRefreshRateFlag(val);
      break;
    case SET_BINDER_DYN_REFRESH_RATE:
      val = va_arg(args, int32_t);
      ForceRefreshRate(UINT32(val));
      break;
    case SET_DISPLAY_MODE:
      val = va_arg(args, int32_t);
      SetDisplayMode(UINT32(val));
      break;
    case SET_QDCM_SOLID_FILL_INFO:
      solid_fill_color = va_arg(args, LayerSolidFill*);
      SetQDCMSolidFillInfo(true, *solid_fill_color);
      break;
    case UNSET_QDCM_SOLID_FILL_INFO:
      solid_fill_color = va_arg(args, LayerSolidFill*);
      SetQDCMSolidFillInfo(false, *solid_fill_color);
      break;
    case SET_QDCM_SOLID_FILL_RECT:
      rect = va_arg(args, LayerRect*);
      solid_fill_rect_ = *rect;
      break;
    default:
      DLOGW("Invalid operation %d", operation);
      va_end(args);
      return -EINVAL;
  }
  va_end(args);
  validated_ = false;
  CacCommitDone(false);

  return 0;
}

DisplayError HWCDisplayBuiltIn::SetDisplayMode(uint32_t mode) {
  DisplayError error = kErrorNone;

  if (display_intf_) {
    error = display_intf_->SetDisplayMode(mode);
    if (error == kErrorNone) {
      DisplayConfigFixedInfo fixed_info = {};
      display_intf_->GetConfig(&fixed_info);
      is_cmd_mode_ = fixed_info.is_cmdmode;
      partial_update_enabled_ = fixed_info.partial_update;
      for (auto hwc_layer : layer_set_) {
        hwc_layer->SetPartialUpdate(partial_update_enabled_);
      }
      client_target_->SetPartialUpdate(partial_update_enabled_);
    }
  }

  return error;
}

void HWCDisplayBuiltIn::SetMetaDataRefreshRateFlag(bool enable) {
  int disable_metadata_dynfps = 0;

  HWCDebugHandler::Get()->GetProperty(DISABLE_METADATA_DYNAMIC_FPS_PROP, &disable_metadata_dynfps);
  if (disable_metadata_dynfps) {
    return;
  }
  use_metadata_refresh_rate_ = enable;
}

void HWCDisplayBuiltIn::SetQDCMSolidFillInfo(bool enable, const LayerSolidFill &color) {
  solid_fill_enable_ = enable;
  solid_fill_color_ = color;
}

void HWCDisplayBuiltIn::ToggleCPUHint(bool set) {
  if (!cpu_hint_) {
    return;
  }

  if (set) {
    cpu_hint_->Set();
  } else {
    cpu_hint_->Reset();
  }
}

int HWCDisplayBuiltIn::HandleSecureSession(const std::bitset<kSecureMax> &secure_sessions,
                                           bool *power_on_pending) {
  if (!power_on_pending) {
    return -EINVAL;
  }

  if (!is_primary_) {
    return HWCDisplay::HandleSecureSession(secure_sessions, power_on_pending);
  }

  if (current_power_mode_ != HWC2::PowerMode::On) {
    return 0;
  }

  if (active_secure_sessions_[kSecureDisplay] != secure_sessions[kSecureDisplay]) {
    SecureEvent secure_event =
        secure_sessions.test(kSecureDisplay) ? kSecureDisplayStart : kSecureDisplayEnd;
    DisplayError err = display_intf_->HandleSecureEvent(secure_event, &layer_stack_);
    if (err != kErrorNone) {
      DLOGE("Set secure event failed");
      return err;
    }

    DLOGI("SecureDisplay state changed from %d to %d for display %" PRIu64 "-%d",
          active_secure_sessions_.test(kSecureDisplay), secure_sessions.test(kSecureDisplay),
          id_, type_);
  }
  active_secure_sessions_ = secure_sessions;
  *power_on_pending = false;
  return 0;
}

void HWCDisplayBuiltIn::ForceRefreshRate(uint32_t refresh_rate) {
  if ((refresh_rate && (refresh_rate < min_refresh_rate_ || refresh_rate > max_refresh_rate_)) ||
      force_refresh_rate_ == refresh_rate) {
    // Cannot honor force refresh rate, as its beyond the range or new request is same
    return;
  }

  force_refresh_rate_ = refresh_rate;

  callbacks_->Refresh(id_);

  return;
}

uint32_t HWCDisplayBuiltIn::GetOptimalRefreshRate(bool one_updating_layer) {
  if (force_refresh_rate_) {
    return force_refresh_rate_;
  } else if (use_metadata_refresh_rate_ && one_updating_layer && metadata_refresh_rate_) {
    return metadata_refresh_rate_;
  }

  DLOGV_IF(kTagClient, "active_refresh_rate_: %d", active_refresh_rate_);
  return active_refresh_rate_;
}

void HWCDisplayBuiltIn::SetIdleTimeoutMs(uint32_t timeout_ms, uint32_t inactive_ms) {
  display_intf_->SetIdleTimeoutMs(timeout_ms, inactive_ms);
  validated_ = false;
}

void HWCDisplayBuiltIn::HandleFrameOutput() {
  if (readback_buffer_queued_) {
    validated_ = false;
  }

  if (frame_capture_buffer_queued_) {
    HandleFrameCapture();
  } else if (dump_output_to_file_) {
    HandleFrameDump();
  }
}

void HWCDisplayBuiltIn::HandleFrameCapture() {
  if (readback_configured_ && (output_buffer_.release_fence_fd >= 0)) {
    frame_capture_status_ = sync_wait(output_buffer_.release_fence_fd, 1000);
    ::close(output_buffer_.release_fence_fd);
    output_buffer_.release_fence_fd = -1;
  }

  frame_capture_buffer_queued_ = false;
  readback_buffer_queued_ = false;
  post_processed_output_ = false;
  readback_configured_ = false;
  output_buffer_ = {};
  cwb_client_ = kCWBClientNone;
}

void HWCDisplayBuiltIn::HandleFrameDump() {
  if (dump_frame_count_) {
    int ret = 0;
    if (output_buffer_.release_fence_fd >= 0) {
      ret = sync_wait(output_buffer_.release_fence_fd, 1000);
      if (ret < 0) {
        DLOGE("sync_wait error errno = %d, desc = %s", errno, strerror(errno));
      }
      ::close(output_buffer_.release_fence_fd);
      output_buffer_.release_fence_fd = -1;
    }

    if (!ret) {
       DumpOutputBuffer(output_buffer_info_, output_buffer_base_, layer_stack_.retire_fence_fd);
       validated_ = false;
     }

    if (0 == (dump_frame_count_ - 1)) {
      dump_output_to_file_ = false;
      // Unmap and Free buffer
      if (munmap(output_buffer_base_, output_buffer_info_.alloc_buffer_info.size) != 0) {
        DLOGE("unmap failed with err %d", errno);
      }
      if (buffer_allocator_->FreeBuffer(&output_buffer_info_) != 0) {
        DLOGE("FreeBuffer failed");
      }

      readback_buffer_queued_ = false;
      post_processed_output_ = false;
      readback_configured_ = false;

      output_buffer_ = {};
      output_buffer_info_ = {};
      output_buffer_base_ = nullptr;
      cwb_client_ = kCWBClientNone;
    }
  }
}

HWC2::Error HWCDisplayBuiltIn::SetFrameDumpConfig(uint32_t count, uint32_t bit_mask_layer_type,
                                                  int32_t format, bool post_processed) {
  HWCDisplay::SetFrameDumpConfig(count, bit_mask_layer_type, format, post_processed);
  dump_output_to_file_ = bit_mask_layer_type & (1 << OUTPUT_LAYER_DUMP);
  DLOGI("output_layer_dump_enable %d", dump_output_to_file_);

  if (dump_output_to_file_) {
    if (cwb_client_ != kCWBClientNone) {
      DLOGE("CWB is in use with client = %d", cwb_client_);
      dump_output_to_file_ = false;
      return HWC2::Error::NoResources;
    }
  }

  if (!count || !dump_output_to_file_ || (output_buffer_info_.alloc_buffer_info.fd >= 0)) {
    return HWC2::Error::None;
  }

  // Allocate and map output buffer
  if (post_processed) {
    // To dump post-processed (DSPP) output, use Panel resolution.
    GetPanelResolution(&output_buffer_info_.buffer_config.width,
                       &output_buffer_info_.buffer_config.height);
  } else {
    // To dump Layer Mixer output, use FrameBuffer resolution.
    GetFrameBufferResolution(&output_buffer_info_.buffer_config.width,
                             &output_buffer_info_.buffer_config.height);
  }

  output_buffer_info_.buffer_config.format = HWCLayer::GetSDMFormat(format, 0);
  output_buffer_info_.buffer_config.buffer_count = 1;
  if (buffer_allocator_->AllocateBuffer(&output_buffer_info_) != 0) {
    DLOGE("Buffer allocation failed");
    output_buffer_info_ = {};
    return HWC2::Error::NoResources;
  }

  void *buffer = mmap(NULL, output_buffer_info_.alloc_buffer_info.size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, output_buffer_info_.alloc_buffer_info.fd, 0);

  if (buffer == MAP_FAILED) {
    DLOGE("mmap failed with err %d", errno);
    buffer_allocator_->FreeBuffer(&output_buffer_info_);
    output_buffer_info_ = {};
    return HWC2::Error::NoResources;
  }

  output_buffer_base_ = buffer;
  const native_handle_t *handle = static_cast<native_handle_t *>(output_buffer_info_.private_data);
  SetReadbackBuffer(handle, -1, post_processed, kCWBClientFrameDump);

  return HWC2::Error::None;
}

int HWCDisplayBuiltIn::FrameCaptureAsync(const BufferInfo &output_buffer_info,
                                         bool post_processed_output) {
  if (cwb_client_ != kCWBClientNone) {
    DLOGE("CWB is in use with client = %d", cwb_client_);
    return -1;
  }

  // Note: This function is called in context of a binder thread and a lock is already held
  if (output_buffer_info.alloc_buffer_info.fd < 0) {
    DLOGE("Invalid fd %d", output_buffer_info.alloc_buffer_info.fd);
    return -1;
  }

  auto panel_width = 0u;
  auto panel_height = 0u;
  auto fb_width = 0u;
  auto fb_height = 0u;

  GetPanelResolution(&panel_width, &panel_height);
  GetFrameBufferResolution(&fb_width, &fb_height);

  if (post_processed_output && (output_buffer_info.buffer_config.width < panel_width ||
                                output_buffer_info.buffer_config.height < panel_height)) {
    DLOGE("Buffer dimensions should not be less than panel resolution");
    return -1;
  } else if (!post_processed_output && (output_buffer_info.buffer_config.width < fb_width ||
                                        output_buffer_info.buffer_config.height < fb_height)) {
    DLOGE("Buffer dimensions should not be less than FB resolution");
    return -1;
  }

  const native_handle_t *buffer = static_cast<native_handle_t *>(output_buffer_info.private_data);
  SetReadbackBuffer(buffer, -1, post_processed_output, kCWBClientColor);
  frame_capture_buffer_queued_ = true;
  frame_capture_status_ = -EAGAIN;

  return 0;
}

DisplayError HWCDisplayBuiltIn::SetDetailEnhancerConfig
                                   (const DisplayDetailEnhancerData &de_data) {
  DisplayError error = kErrorNotSupported;

  if (display_intf_) {
    error = display_intf_->SetDetailEnhancerData(de_data);
    validated_ = false;
  }
  return error;
}

DisplayError HWCDisplayBuiltIn::SetHWDetailedEnhancerConfig(void *params) {
  DisplayError err = kErrorNone;
  DisplayDetailEnhancerData de_data;

  PPDETuningCfgData *de_tuning_cfg_data = reinterpret_cast<PPDETuningCfgData *>(params);
  if (de_tuning_cfg_data->cfg_pending) {
    if (!de_tuning_cfg_data->cfg_en) {
      de_data.enable = 0;
      DLOGV_IF(kTagQDCM, "Disable DE config");
    } else {
      de_data.override_flags = kOverrideDEEnable;
      de_data.enable = 1;

      DLOGV_IF(kTagQDCM, "Enable DE: flags %u, sharp_factor %d, thr_quiet %d, thr_dieout %d, "
        "thr_low %d, thr_high %d, clip %d, quality %d, content_type %d, de_blend %d",
        de_tuning_cfg_data->params.flags, de_tuning_cfg_data->params.sharp_factor,
        de_tuning_cfg_data->params.thr_quiet, de_tuning_cfg_data->params.thr_dieout,
        de_tuning_cfg_data->params.thr_low, de_tuning_cfg_data->params.thr_high,
        de_tuning_cfg_data->params.clip, de_tuning_cfg_data->params.quality,
        de_tuning_cfg_data->params.content_type, de_tuning_cfg_data->params.de_blend);

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagSharpFactor) {
        de_data.override_flags |= kOverrideDESharpen1;
        de_data.sharp_factor = de_tuning_cfg_data->params.sharp_factor;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagClip) {
        de_data.override_flags |= kOverrideDEClip;
        de_data.clip = de_tuning_cfg_data->params.clip;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagThrQuiet) {
        de_data.override_flags |= kOverrideDEThrQuiet;
        de_data.thr_quiet = de_tuning_cfg_data->params.thr_quiet;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagThrDieout) {
        de_data.override_flags |= kOverrideDEThrDieout;
        de_data.thr_dieout = de_tuning_cfg_data->params.thr_dieout;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagThrLow) {
        de_data.override_flags |= kOverrideDEThrLow;
        de_data.thr_low = de_tuning_cfg_data->params.thr_low;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagThrHigh) {
        de_data.override_flags |= kOverrideDEThrHigh;
        de_data.thr_high = de_tuning_cfg_data->params.thr_high;
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagContentQualLevel) {
        switch (de_tuning_cfg_data->params.quality) {
          case kDeContentQualLow:
            de_data.quality_level = kContentQualityLow;
            break;
          case kDeContentQualMedium:
            de_data.quality_level = kContentQualityMedium;
            break;
          case kDeContentQualHigh:
            de_data.quality_level = kContentQualityHigh;
            break;
          case kDeContentQualUnknown:
          default:
            de_data.quality_level = kContentQualityUnknown;
            break;
        }
      }

      if (de_tuning_cfg_data->params.flags & kDeTuningFlagDeBlend) {
        de_data.override_flags |= kOverrideDEBlend;
        de_data.de_blend = de_tuning_cfg_data->params.de_blend;
      }
    }
    err = SetDetailEnhancerConfig(de_data);
    if (err) {
      DLOGW("SetDetailEnhancerConfig failed. err = %d", err);
    }
    de_tuning_cfg_data->cfg_pending = false;
  }
  return err;
}

DisplayError HWCDisplayBuiltIn::ControlPartialUpdate(bool enable, uint32_t *pending) {
  DisplayError error = kErrorNone;

  if (display_intf_) {
    error = display_intf_->ControlPartialUpdate(enable, pending);
    validated_ = false;
  }

  return error;
}

DisplayError HWCDisplayBuiltIn::DisablePartialUpdateOneFrame() {
  DisplayError error = kErrorNone;

  if (display_intf_) {
    error = display_intf_->DisablePartialUpdateOneFrame();
    validated_ = false;
  }

  return error;
}

HWC2::Error HWCDisplayBuiltIn::SetDisplayedContentSamplingEnabledVndService(bool enabled) {
  std::unique_lock<decltype(sampling_mutex)> lk(sampling_mutex);
  vndservice_sampling_vote = enabled;
  if (api_sampling_vote || vndservice_sampling_vote) {
    histogram.start();
    display_intf_->colorSamplingOn();
  } else {
    display_intf_->colorSamplingOff();
    histogram.stop();
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetDisplayedContentSamplingEnabled(int32_t enabled,
                                                                  uint8_t component_mask,
                                                                  uint64_t max_frames) {
  if ((enabled != HWC2_DISPLAYED_CONTENT_SAMPLING_ENABLE) &&
      (enabled != HWC2_DISPLAYED_CONTENT_SAMPLING_DISABLE))
    return HWC2::Error::BadParameter;

  std::unique_lock<decltype(sampling_mutex)> lk(sampling_mutex);
  if (enabled == HWC2_DISPLAYED_CONTENT_SAMPLING_ENABLE) {
    api_sampling_vote = true;
  } else {
    api_sampling_vote = false;
  }

  auto start = api_sampling_vote || vndservice_sampling_vote;
  if (start && max_frames == 0) {
    histogram.start();
    display_intf_->colorSamplingOn();
  } else if (start) {
    histogram.start(max_frames);
    display_intf_->colorSamplingOn();
  } else {
    display_intf_->colorSamplingOff();
    histogram.stop();
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::GetDisplayedContentSamplingAttributes(
    int32_t *format, int32_t *dataspace, uint8_t *supported_components) {
  return histogram.getAttributes(format, dataspace, supported_components);
}

HWC2::Error HWCDisplayBuiltIn::GetDisplayedContentSample(
    uint64_t max_frames, uint64_t timestamp, uint64_t *numFrames,
    int32_t samples_size[NUM_HISTOGRAM_COLOR_COMPONENTS],
    uint64_t *samples[NUM_HISTOGRAM_COLOR_COMPONENTS]) {
  histogram.collect(max_frames, timestamp, samples_size, samples, numFrames);
  return HWC2::Error::None;
}

DisplayError HWCDisplayBuiltIn::SetMixerResolution(uint32_t width, uint32_t height) {
  DisplayError error = display_intf_->SetMixerResolution(width, height);
  callbacks_->Refresh(id_);
  validated_ = false;
  return error;
}

DisplayError HWCDisplayBuiltIn::GetMixerResolution(uint32_t *width, uint32_t *height) {
  return display_intf_->GetMixerResolution(width, height);
}

HWC2::Error HWCDisplayBuiltIn::SetQSyncMode(QSyncMode qsync_mode) {
  // Client needs to ensure that config change and qsync mode change
  // are not triggered in the same drawcycle.
  if (pending_config_) {
    DLOGE("Failed to set qsync mode. Pending active config transition");
    return HWC2::Error::Unsupported;
  }

  auto err = display_intf_->SetQSyncMode(qsync_mode);
  if (err != kErrorNone) {
    return HWC2::Error::Unsupported;
  }

  validated_ = false;
  return HWC2::Error::None;
}

DisplayError HWCDisplayBuiltIn::ControlIdlePowerCollapse(bool enable, bool synchronous) {
  DisplayError error = kErrorNone;

  if (display_intf_) {
    error = display_intf_->ControlIdlePowerCollapse(enable, synchronous);
    validated_ = false;
  }
  return error;
}

DisplayError HWCDisplayBuiltIn::SetDynamicDSIClock(uint64_t bitclk) {
  DisablePartialUpdateOneFrame();
  DisplayError error = display_intf_->SetDynamicDSIClock(bitclk);
  if (error != kErrorNone) {
    DLOGE(" failed: Clk: %" PRIu64 " Error: %d", bitclk, error);
    return error;
  }

  callbacks_->Refresh(id_);
  validated_ = false;

  return kErrorNone;
}

DisplayError HWCDisplayBuiltIn::GetDynamicDSIClock(uint64_t *bitclk) {
  if (display_intf_) {
    return display_intf_->GetDynamicDSIClock(bitclk);
  }

  return kErrorNotSupported;
}

DisplayError HWCDisplayBuiltIn::GetSupportedDSIClock(std::vector<uint64_t> *bitclk_rates) {
  if (display_intf_) {
    return display_intf_->GetSupportedDSIClock(bitclk_rates);
  }

  return kErrorNotSupported;
}

HWC2::Error HWCDisplayBuiltIn::UpdateDisplayId(hwc2_display_t id) {
  id_ = id;
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetPendingRefresh() {
  pending_refresh_ = true;
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetPanelBrightness(float brightness) {
  DisplayError ret = display_intf_->SetPanelBrightness(brightness);
  if (ret != kErrorNone) {
    return HWC2::Error::NoResources;
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::GetPanelBrightness(float *brightness) {
  DisplayError ret = display_intf_->GetPanelBrightness(brightness);
  if (ret != kErrorNone) {
    return HWC2::Error::NoResources;
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::GetPanelMaxBrightness(uint32_t *max_brightness_level) {
  DisplayError ret = display_intf_->GetPanelMaxBrightness(max_brightness_level);
  if (ret != kErrorNone) {
    return HWC2::Error::NoResources;
  }

  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetBLScale(uint32_t level) {
  DisplayError ret = display_intf_->SetBLScale(level);
  if (ret != kErrorNone) {
    return HWC2::Error::NoResources;
  }
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::UpdatePowerMode(HWC2::PowerMode mode) {
  current_power_mode_ = mode;
  validated_ = false;
  return HWC2::Error::None;
}

HWC2::Error HWCDisplayBuiltIn::SetClientTarget(buffer_handle_t target, int32_t acquire_fence,
                                               int32_t dataspace, hwc_region_t damage) {
  HWC2::Error error = HWCDisplay::SetClientTarget(target, acquire_fence, dataspace, damage);
  if (error != HWC2::Error::None) {
    return error;
  }

  // windowed_display and dynamic scaling are not supported.
  if (windowed_display_) {
    return HWC2::Error::None;
  }

  Layer *sdm_layer = client_target_->GetSDMLayer();
  uint32_t fb_width = 0, fb_height = 0;

  GetFrameBufferResolution(&fb_width, &fb_height);

  if (fb_width != sdm_layer->input_buffer.unaligned_width ||
      fb_height != sdm_layer->input_buffer.unaligned_height) {
    if (SetFrameBufferConfig(sdm_layer->input_buffer.unaligned_width,
                             sdm_layer->input_buffer.unaligned_height)) {
      return HWC2::Error::BadParameter;
    }
  }

  return HWC2::Error::None;
}

bool HWCDisplayBuiltIn::IsSmartPanelConfig(uint32_t config_id) {
  if (config_id < hwc_config_map_.size()) {
    uint32_t index = hwc_config_map_.at(config_id);
    return variable_config_map_.at(index).smart_panel;
  }

  return false;
}

bool HWCDisplayBuiltIn::HasSmartPanelConfig(void) {
  if (!enable_poms_during_doze_) {
    uint32_t config = 0;
    GetActiveDisplayConfig(&config);
    return IsSmartPanelConfig(config);
  }

  for (auto &config : variable_config_map_) {
    if (config.second.smart_panel) {
      return true;
    }
  }

  return false;
}

int HWCDisplayBuiltIn::Deinit() {
  // Destory color convert instance. This destroys thread and underlying GL resources.
  if (gl_layer_stitch_) {
    layer_stitch_task_.PerformTask(LayerStitchTaskCode::kCodeDestroyInstance, nullptr);
  }

  histogram.stop();

  // free the WB buffer
  buffer_allocator_->FreeBuffer(&wb_buffer_info_);

  // stop the WB kickoff thread
  pthread_mutex_lock(&wb_lock_);
  exit_wb_thread_ = true;
  pthread_cond_signal(&wb_cv_);
  pthread_mutex_unlock(&wb_lock_);

  pthread_join(wb_kickoff_thread_, NULL);

  delete wb_op_layer_;
  delete new_client_target_;

  sec_layer_map_.clear();
  sec_layer_set_.clear();

  return HWCDisplay::Deinit();
}

void HWCDisplayBuiltIn::OnTask(const LayerStitchTaskCode &task_code,
                               SyncTask<LayerStitchTaskCode>::TaskContext *task_context) {
  switch (task_code) {
    case LayerStitchTaskCode::kCodeGetInstance: {
        gl_layer_stitch_ = GLLayerStitch::GetInstance(false /* Non-secure */);
      }
      break;
    case LayerStitchTaskCode::kCodeStitch: {
        DTRACE_SCOPED();
        LayerStitchContext* ctx = reinterpret_cast<LayerStitchContext*>(task_context);
        gl_layer_stitch_->Blit(ctx->stitch_params, &(ctx->release_fence_fd));
      }
      break;
    case LayerStitchTaskCode::kCodeDestroyInstance: {
        if (gl_layer_stitch_) {
          GLLayerStitch::Destroy(gl_layer_stitch_);
        }
      }
      break;
  }
}

bool HWCDisplayBuiltIn::InitLayerStitch() {
  if (!is_primary_) {
    // Disable on all non-primary builtins.
    DLOGI("Non-primary builtin.");
    disable_layer_stitch_ = true;
    return true;
  }

  // Disable by default.
  int value = 1;
  Debug::Get()->GetProperty(DISABLE_LAYER_STITCH, &value);
  disable_layer_stitch_ = (value == 1);

  if (disable_layer_stitch_) {
    DLOGI("Layer Stitch Disabled !!!");
    return true;
  }

  // Initialize stitch context. This will be non-secure.
  layer_stitch_task_.PerformTask(LayerStitchTaskCode::kCodeGetInstance, nullptr);
  if (gl_layer_stitch_ == nullptr) {
    DLOGE("Failed to get LayerStitch Instance");
    return false;
  }

  if (!AllocateStitchBuffer()) {
    return true;
  }

  stitch_target_ = new HWCLayer(id_, static_cast<HWCBufferAllocator *>(buffer_allocator_));

  // Populate buffer params and pvt handle.
  InitStitchTarget();

  DLOGI("Created LayerStitch instance: %p", gl_layer_stitch_);

  return true;
}

bool HWCDisplayBuiltIn::AllocateStitchBuffer() {
  // Buffer dimensions: FB width * (1.5 * height)

  DisplayError error = display_intf_->GetFrameBufferConfig(&fb_config_);
  if (error != kErrorNone) {
    DLOGE("Get frame buffer config failed. Error = %d", error);
    return false;
  }

  BufferConfig &config = buffer_info_.buffer_config;
  config.width = fb_config_.x_pixels;
  config.height = fb_config_.y_pixels * kBufferHeightFactor;

  // By default UBWC is enabled and below property is global enable/disable for all
  // buffers allocated through gralloc , including framebuffer targets.
  int ubwc_disabled = 0;
  HWCDebugHandler::Get()->GetProperty(DISABLE_UBWC_PROP, &ubwc_disabled);
  config.format = ubwc_disabled ? kFormatRGBA8888 : kFormatRGBA8888Ubwc;

  config.gfx_client = true;

  // Populate default params.
  config.secure = false;
  config.cache = false;
  config.secure_camera = false;

  error = buffer_allocator_->AllocateBuffer(&buffer_info_);

  if (error != kErrorNone) {
    DLOGE("Failed to allocate buffer. Error: %d", error);
    return false;
  }

  return true;
}

void HWCDisplayBuiltIn::InitStitchTarget() {
  LayerBuffer buffer = {};
  buffer.planes[0].fd = buffer_info_.alloc_buffer_info.fd;
  buffer.planes[0].offset = 0;
  buffer.planes[0].stride = buffer_info_.alloc_buffer_info.stride;
  buffer.size = buffer_info_.alloc_buffer_info.size;
  buffer.handle_id = buffer_info_.alloc_buffer_info.id;
  buffer.width = buffer_info_.alloc_buffer_info.aligned_width;
  buffer.height = buffer_info_.alloc_buffer_info.aligned_height;
  buffer.unaligned_width = fb_config_.x_pixels;
  buffer.unaligned_height = fb_config_.y_pixels * kBufferHeightFactor;
  buffer.format = buffer_info_.alloc_buffer_info.format;

  Layer *sdm_stitch_target = stitch_target_->GetSDMLayer();
  sdm_stitch_target->composition = kCompositionStitchTarget;
  sdm_stitch_target->input_buffer = buffer;
  sdm_stitch_target->input_buffer.buffer_id = reinterpret_cast<uint64_t>(buffer_info_.private_data);
}

void HWCDisplayBuiltIn::AppendStitchLayer() {
  if (disable_layer_stitch_) {
    return;
  }

  // Append stitch target buffer to layer stack.
  Layer *sdm_stitch_target = stitch_target_->GetSDMLayer();
  sdm_stitch_target->composition = kCompositionStitchTarget;
  sdm_stitch_target->dst_rect = {0, 0, FLOAT(fb_config_.x_pixels), FLOAT(fb_config_.y_pixels)};
  layer_stack_.layers.push_back(sdm_stitch_target);
}

DisplayError HWCDisplayBuiltIn::HistogramEvent(int fd, uint32_t blob_id) {
  histogram.notify_histogram_event(fd, blob_id);
  return kErrorNone;
}

int HWCDisplayBuiltIn::PostInit() {
  auto status = InitLayerStitch();
  if (!status) {
    DLOGW("Failed to initialize Layer Stitch context");
    // Disable layer stitch.
    disable_layer_stitch_ = true;
  }

  return 0;
}

bool HWCDisplayBuiltIn::HasReadBackBufferSupport() {
  DisplayConfigFixedInfo fixed_info = {};
  display_intf_->GetConfig(&fixed_info);

  return fixed_info.readback_supported;
}

int32_t HWCDisplayBuiltIn::SetCAC(bool enable, float red, float green, float blue,
                                  PanelOrientation orientation) {
  uint32_t config_index = 0;
  DisplayConfigVariableInfo attr = {};
  GetDisplayAttributesForConfig(INT(config_index), &attr);

  // Turn off CAC 4k@120Hz under landscape case
  if (current_refresh_rate_ >= 120 && attr.x_pixels >= 4320
      && (attr.x_pixels > attr.y_pixels)) {
    DLOGW("Display %d-%d, Current refresh rate is %d, resolution is %dx%d",
          sdm_id_, type_, current_refresh_rate_, attr.x_pixels, attr.y_pixels);
    return -1;
  }

  HWCSession *hwc_session = HWCSession::GetInstance();
  HWCDisplayVirtualDPU *wb_display = nullptr;
  if (hwc_session->wb_display_) {
    InitCacResources(attr.x_pixels, attr.y_pixels);
    wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
      (hwc_session->GetDisplay(hwc_session->wb_display_));

    auto err = wb_display->SetFrameRate(current_refresh_rate_);
    if (err != HWC2::Error::None) {
      DLOGE("Failed for display %" PRIu64 " %d-%d, fps = %d, errno = %d", id_, sdm_id_, type_,
            current_refresh_rate_, err);
    }

    int32_t error = wb_display->SetCAC(enable, red, green, blue, GetPanelOrientation());
    if (error != kErrorNone) {
      DLOGE("Failed for display %" PRIu64 " %d-%d, enable = %d, red = %f, green = %f, blue = %f, "
            "errno = %d, desc = %s", id_, sdm_id_, type_, enable, red, green, blue, error,
            strerror(error));
      return -1;
    }
  }

  // Pass CAC enablement info to Builtin for fence management
  display_intf_->SetCAC(enable, red, green, blue, orientation);

  enable_cac_ = enable;
  bool is_wb_cac_in_use = IsWBCacInUse();
  if (!enable_cac_) {
    // Disable lineptr events on this display when wb is in use.
    if (is_wb_cac_in_use) {
      display_intf_->SetLinePtrState(false, 0);
    }
    if (wb_display != nullptr) {
      // Reset layer stack of WB display to allow it to be safely destroyed.
      HWCDisplay::HWCLayerStack primary_layer_stack = {};
      wb_display->GetLayerStack(&primary_layer_stack);
      wb_display->ClearLayerStack();
      SetLayerStack(&primary_layer_stack);
    }
  } else {
    // Enable lineptr events at every y_pixels/2 on this display.
    uint32_t config_index = 0;
    GetActiveDisplayConfig(&config_index);
    DisplayConfigVariableInfo attr = {};
    GetDisplayAttributesForConfig(INT(config_index), &attr);
    if (is_wb_cac_in_use) {
      display_intf_->SetLinePtrState(true, attr.y_pixels/2);
    }
  }

  validated_ = false;
  frame_split_rect_ = {};  // clear the frame_split_rect_
  CacCommitDone(false);
  callbacks_->Refresh(id_);

  return 0;
}

void HWCDisplayBuiltIn::UpdateFramerateForCAC(uint32_t fps) {
  HWCSession *hwc_session = HWCSession::GetInstance();
  HWCDisplayVirtualDPU *wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
                                     (hwc_session->GetDisplay(hwc_session->wb_display_));
  if (!wb_display) {
    DLOGE("Return no WB display for display = %lu", id_);
    return;
  }

  auto error = wb_display->SetFrameRate(fps);
  if (error != HWC2::Error::None) {
    DLOGE("Failed to set fps for display %" PRIu64 " %d-%d, fps = %d, errno = %d", id_, sdm_id_,
          type_, fps, error);
    return;
  }
  // Re-constitue WB CAC cache for the new fps.
  wb_display->CacCommitDone(IsCacCommitDone());
  return;
}

void *HWCDisplayBuiltIn::WBKickOffThread(void *context) {
  if (context) {
    return reinterpret_cast<HWCDisplayBuiltIn *>(context)->PerformWBKickOff();
  }
  return NULL;
}

void HWCDisplayBuiltIn::HandleLinePtrEvent() {
  if (!enable_cac_) {
    return;
  }

  if (!cac_commit_done_) {
    callbacks_->Refresh(id_);
    return;
  }

  pthread_mutex_lock(&wb_lock_);
  pthread_cond_signal(&wb_cv_);
  pthread_mutex_unlock(&wb_lock_);
}

void *HWCDisplayBuiltIn::PerformWBKickOff() {

  struct sched_param param = {};
  param.sched_priority = 50;
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
  while (1) {
    pthread_mutex_lock(&wb_lock_);
    pthread_cond_wait(&wb_cv_, &wb_lock_);
    if (exit_wb_thread_) {
      pthread_mutex_unlock(&wb_lock_);
      return nullptr;
    }
    DTRACE_SCOPED();
    SEQUENCE_WAIT_SCOPE_LOCK(HWCSession::locker_[id_]);
    for (auto hwc_layer : layer_set_) {
      auto layer = hwc_layer->GetSDMLayer();
      if (layer->update_mask[kSecurity])  {
        DLOGI_IF(kTagClient, "Display %d-%d Security level changed", sdm_id_, type_);
        CacCommitDone(false);
      }
    }
    if (cac_commit_done_) {
      ValidateAndCommitWB();
    }
    pthread_mutex_unlock(&wb_lock_);
  }
  return nullptr;
}

void HWCDisplayBuiltIn::CacCommitDone(bool cac_commit_done) {
  DLOGI("Setting CAC Commit done = %s.", cac_commit_done ? "true" : "false");
  cac_commit_done_ = cac_commit_done;
  if (IsWBCacInUse()) {
    HWCSession *hwc_session = HWCSession::GetInstance();
    HWCDisplayVirtualDPU *wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
                                       (hwc_session->GetDisplay(hwc_session->wb_display_));
    if (!wb_display) {
      DLOGE("Return no WB display for display = %lu", id_);
      return;
    }

    wb_display->CacCommitDone(cac_commit_done);
  }
}

bool HWCDisplayBuiltIn::IsCacCommitDone() {
  if (cac_commit_done_) {
    for (auto hwc_layer : layer_set_) {
      auto layer = hwc_layer->GetSDMLayer();
      layer->composition = kCompositionSDE;
    }
    DLOGI("cac commit done - Marking layers for GPU Bypass");
  }
  return cac_commit_done_;
}

int32_t HWCDisplayBuiltIn::SetCACEyeConfig(const CACEyeConfig &left,
                                           const CACEyeConfig &right) {
  int32_t error = HWCDisplay::SetCACEyeConfig(left, right);
  if (error != kErrorNone) {
    DLOGE("Failed to set IPD params for display %" PRIu64 " %d-%d, errno = %d", id_, sdm_id_,
          type_, error);
    return -1;
  }

  HWCSession *hwc_session = HWCSession::GetInstance();
  if (IsWBCacInUse()) {
    HWCDisplayVirtualDPU *wb_display = reinterpret_cast<HWCDisplayVirtualDPU *>
                          (hwc_session->GetDisplay(hwc_session->wb_display_));
    error = wb_display->SetCACEyeConfig(left, right);
    if (error != kErrorNone) {
      DLOGE("Failed to set IPD params for display %" PRIu64 " %d-%d, errno = %d", id_, sdm_id_,
            type_, error);
      return -1;
    }
  }
  validated_ = false;
  CacCommitDone(false);
  callbacks_->Refresh(id_);

  return 0;
}

int32_t HWCDisplayBuiltIn::SetSkewVsync(uint32_t skew_vsync_val) {
  DisplayError error = display_intf_->SetSkewVsync(skew_vsync_val);
  if (error != kErrorNone) {
    DLOGE("Failed for display %" PRIu64 " %d-%d, skew_vsync = %d", id_, sdm_id_, type_,
          skew_vsync_val);
    return -1;
  }

  return 0;
};

}  // namespace sdm
