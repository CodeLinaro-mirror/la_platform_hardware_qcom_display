/*
* Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation. nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
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
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <core/buffer_allocator.h>
#include <utils/debug.h>
#include <utils/constants.h>
#include <sync/sync.h>
#include <vector>
#include <string>
#include <QtiGralloc.h>
#include <aidlcommonsupport/NativeHandle.h>

#include "hwc_buffer_sync_handler.h"
#include "hwc_session.h"
#include "hwc_debugger.h"

#define __CLASS__ "HWCSession"

typedef ::aidl::vendor::qti::hardware::display::config::DisplayType AIDLDisplayType;

namespace sdm {

int MapDisplayType(DispType dpy) {
  switch (dpy) {
    case DispType::kPrimary:
      return qdutils::DISPLAY_PRIMARY;

    case DispType::kExternal:
      return qdutils::DISPLAY_EXTERNAL;

    case DispType::kVirtual:
      return qdutils::DISPLAY_VIRTUAL;

    case DispType::kBuiltIn2:
      return qdutils::DISPLAY_BUILTIN_2;

    default:
      break;
  }

  return -EINVAL;
}

AIDLDisplayType MapDisplayId(int disp_id) {
  switch (disp_id) {
    case qdutils::DISPLAY_PRIMARY:
      return AIDLDisplayType::PRIMARY;

    case qdutils::DISPLAY_EXTERNAL:
      return AIDLDisplayType::EXTERNAL;

    case qdutils::DISPLAY_VIRTUAL:
      return AIDLDisplayType::VIRTUAL;

    case qdutils::DISPLAY_BUILTIN_2:
      return AIDLDisplayType::BUILTIN2;

    default:
      break;
  }

  return AIDLDisplayType::INVALID;
}

bool WaitForResourceNeeded(PowerMode prev_mode, PowerMode new_mode) {
  return ((prev_mode == PowerMode::OFF) &&
          (new_mode == PowerMode::ON || new_mode == PowerMode::DOZE));
}

HWCDisplay::DisplayStatus MapExternalStatus(DisplayConfig::ExternalStatus status) {
  switch (status) {
    case DisplayConfig::ExternalStatus::kOffline:
      return HWCDisplay::kDisplayStatusOffline;

    case DisplayConfig::ExternalStatus::kOnline:
      return HWCDisplay::kDisplayStatusOnline;

    case DisplayConfig::ExternalStatus::kPause:
      return HWCDisplay::kDisplayStatusPause;

    case DisplayConfig::ExternalStatus::kResume:
      return HWCDisplay::kDisplayStatusResume;

    default:
      break;
  }

  return HWCDisplay::kDisplayStatusInvalid;
}

int HWCSession::SetDisplayStatus(int disp_id, HWCDisplay::DisplayStatus status) {
  int disp_idx = GetDisplayIndex(disp_id);
  int err = -EINVAL;
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  if (disp_idx == qdutils::DISPLAY_PRIMARY) {
    DLOGE("Not supported for this display");
    return err;
  }

  {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
    if (!hwc_display_[disp_idx]) {
      DLOGW("Display is not connected");
      return err;
    }
    DLOGI("Display = %d, Status = %d", disp_idx, status);
    err = hwc_display_[disp_idx]->SetDisplayStatus(status);
    if (err != 0) {
      return err;
    }
  }

  if (status == HWCDisplay::kDisplayStatusResume || status == HWCDisplay::kDisplayStatusPause) {
    Display active_builtin_disp_id = GetActiveBuiltinDisplay();
    if (active_builtin_disp_id < HWCCallbacks::kNumRealDisplays) {
      callbacks_.Refresh(active_builtin_disp_id);
    }
  }

  return err;
}

int HWCSession::GetConfigCount(int disp_id, uint32_t *count) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);

  if (hwc_display_[disp_idx]) {
    return hwc_display_[disp_idx]->GetDisplayConfigCount(count);
  }

  return -EINVAL;
}

int HWCSession::GetActiveConfigIndex(int disp_id, uint32_t *config) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);

  if (hwc_display_[disp_idx]) {
    return hwc_display_[disp_idx]->GetActiveDisplayConfig(config);
  }

  return -EINVAL;
}

int HWCSession::SetActiveConfigIndex(int disp_id, uint32_t config) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  int error = -EINVAL;
  if (hwc_display_[disp_idx]) {
    error = hwc_display_[disp_idx]->SetActiveDisplayConfig(config);
    if (!error) {
      callbacks_.Refresh(0);
    }
  }

  return error;
}

int HWCSession::SetNoisePlugInOverride(int32_t disp_id, bool override_en, int32_t attn,
                                       int32_t noise_zpos) {
  int32_t disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  int32_t error = -EINVAL;
  if (hwc_display_[disp_idx]) {
    error = hwc_display_[disp_idx]->SetNoisePlugInOverride(override_en, attn, noise_zpos);
  }

  return error;
}

int HWCSession::MinHdcpEncryptionLevelChanged(int disp_id, uint32_t min_enc_level) {
  DLOGI("Display %d", disp_id);

  // SSG team hardcoded disp_id as external because it applies to external only but SSG team sends
  // this level irrespective of external connected or not. So to honor the call, make disp_id to
  // primary & set level.
  disp_id = HWC_DISPLAY_PRIMARY;
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
  HWCDisplay *hwc_display = hwc_display_[disp_idx];
  if (!hwc_display) {
    DLOGE("Display = %d is not connected.", disp_idx);
    return -EINVAL;
  }

  return hwc_display->OnMinHdcpEncryptionLevelChange(min_enc_level);
}

int HWCSession::ControlPartialUpdate(int disp_id, bool enable) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  if (disp_idx != HWC_DISPLAY_PRIMARY) {
    DLOGW("CONTROL_PARTIAL_UPDATE is not applicable for display = %d", disp_idx);
    return -EINVAL;
  }
  {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[disp_idx]);
    HWCDisplay *hwc_display = hwc_display_[HWC_DISPLAY_PRIMARY];
    if (!hwc_display) {
      DLOGE("primary display object is not instantiated");
      return -EINVAL;
    }

    uint32_t pending = 0;
    DisplayError hwc_error = hwc_display->ControlPartialUpdate(enable, &pending);
    if (hwc_error == kErrorNone) {
      if (!pending) {
        return 0;
      }
    } else if (hwc_error == kErrorNotSupported) {
      return 0;
    } else {
      return -EINVAL;
    }
  }

  // Todo(user): Unlock it before sending events to client. It may cause deadlocks in future.
  // Wait until partial update control is complete
  int error = WaitForCommitDone(HWC_DISPLAY_PRIMARY, kClientPartialUpdate);
  if (error != 0) {
    DLOGW("%s Partial update failed with error %d", enable ? "Enable" : "Disable", error);
  }

  return error;
}

int HWCSession::ToggleScreenUpdate(bool on) {
  Display active_builtin_disp_id = GetActiveBuiltinDisplay();

  if (active_builtin_disp_id >= HWCCallbacks::kNumDisplays) {
    DLOGE("No active displays");
    return -EINVAL;
  }
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[active_builtin_disp_id]);

  int error = -EINVAL;
  if (hwc_display_[active_builtin_disp_id]) {
    error = hwc_display_[active_builtin_disp_id]->ToggleScreenUpdates(on);
    if (error) {
      DLOGE("Failed to toggle screen updates = %d. Display = %" PRIu64 ", Error = %d", on,
            active_builtin_disp_id, error);
    }
  } else {
    DLOGW("Display = %" PRIu64 " is not connected.", active_builtin_disp_id);
  }

  return error;
}

int HWCSession::SetIdleTimeout(uint32_t value) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  int inactive_ms = IDLE_TIMEOUT_INACTIVE_MS;
  Debug::Get()->GetProperty(IDLE_TIME_INACTIVE_PROP, &inactive_ms);
  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    hwc_display_[HWC_DISPLAY_PRIMARY]->SetIdleTimeoutMs(value, inactive_ms);
    idle_time_inactive_ms_ = inactive_ms;
    idle_time_active_ms_ = value;
    return 0;
  }

  DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
  return -ENODEV;
}

int HWCSession::SetCameraLaunchStatus(uint32_t on) {
  if (!core_intf_) {
    DLOGW("core_intf_ not initialized.");
    return -ENOENT;
  }

  HWBwModes mode = on > 0 ? kBwVFEOn : kBwVFEOff;

  if (core_intf_->SetMaxBandwidthMode(mode) != kErrorNone) {
    return -EINVAL;
  }

  // trigger invalidate to apply new bw caps.
  callbacks_.Refresh(0);

  return 0;
}

int HWCSession::DisplayBWTransactionPending(bool *status) {
  SEQUENCE_WAIT_SCOPE_LOCK(locker_[HWC_DISPLAY_PRIMARY]);

  if (hwc_display_[HWC_DISPLAY_PRIMARY]) {
    if (sync_wait(bw_mode_release_fd_, 0) < 0) {
      DLOGI("bw_transaction_release_fd is not yet signaled: err= %s", strerror(errno));
      *status = false;
    }

    return 0;
  }

  DLOGW("Display = %d is not connected.", HWC_DISPLAY_PRIMARY);
  return -ENODEV;
}

int HWCSession::ControlIdlePowerCollapse(bool enable, bool synchronous) {
  Display active_builtin_disp_id = GetActiveBuiltinDisplay();
  if (active_builtin_disp_id >= HWCCallbacks::kNumDisplays) {
    DLOGW("No active displays");
    return 0;
  }
  bool needs_refresh = false;
  {
    SEQUENCE_WAIT_SCOPE_LOCK(locker_[active_builtin_disp_id]);
    if (hwc_display_[active_builtin_disp_id]) {
      if (!enable) {
        if (!idle_pc_ref_cnt_) {
          auto err =
              hwc_display_[active_builtin_disp_id]->ControlIdlePowerCollapse(enable, synchronous);
          if (err != kErrorNone) {
            return (err == kErrorNotSupported) ? 0 : -EINVAL;
          }
          needs_refresh = true;
        }
        idle_pc_ref_cnt_++;
      } else if (idle_pc_ref_cnt_ > 0) {
        if (!(idle_pc_ref_cnt_ - 1)) {
          auto err =
              hwc_display_[active_builtin_disp_id]->ControlIdlePowerCollapse(enable, synchronous);
          if (err != kErrorNone) {
            return (err == kErrorNotSupported) ? 0 : -EINVAL;
          }
        }
        idle_pc_ref_cnt_--;
      }
    } else {
      DLOGW("Display = %d is not connected.", UINT32(active_builtin_disp_id));
      return -ENODEV;
    }
  }
  if (needs_refresh) {
    int ret = WaitForCommitDone(active_builtin_disp_id, kClientIdlepowerCollapse);
    if (ret != 0) {
      DLOGW("%s Idle PC failed with error %d", enable ? "Enable" : "Disable", ret);
      return -EINVAL;
    }
  }

  DLOGI("Idle PC %s!!", enable ? "enabled" : "disabled");
  return 0;
}

int HWCSession::IsWbUbwcSupported(bool *value) {
  HWDisplaysInfo hw_displays_info = {};
  DisplayError error = core_intf_->GetDisplaysStatus(&hw_displays_info);
  if (error != kErrorNone) {
    return -EINVAL;
  }

  for (auto &iter : hw_displays_info) {
    auto &info = iter.second;
    if (info.display_type == kVirtual && info.is_wb_ubwc_supported) {
      *value = 1;
    }
  }

  return error;
}

int32_t HWCSession::getDisplayBrightness(uint32_t display, float *brightness) {
  if (!brightness) {
    return INT32(HWC3::Error::BadParameter);
  }

  if (display >= HWCCallbacks::kNumDisplays) {
    return INT32(HWC3::Error::BadDisplay);
  }

  SEQUENCE_WAIT_SCOPE_LOCK(locker_[display]);
  int32_t error = -EINVAL;
  *brightness = -1.0f;

  HWCDisplay *hwc_display = hwc_display_[display];
  if (hwc_display && hwc_display_[display]->GetDisplayClass() == DISPLAY_CLASS_BUILTIN) {
    error = INT32(hwc_display_[display]->GetPanelBrightness(brightness));
    if (error) {
      DLOGE("Failed to get the panel brightness. Error = %d", error);
    }
  }

  return error;
}

int32_t HWCSession::getDisplayMaxBrightness(uint32_t display, uint32_t *max_brightness_level) {
  if (!max_brightness_level) {
    return INT32(HWC3::Error::BadParameter);
  }

  if (display >= HWCCallbacks::kNumDisplays) {
    return INT32(HWC3::Error::BadDisplay);
  }

  int32_t error = -EINVAL;
  HWCDisplay *hwc_display = hwc_display_[display];
  if (hwc_display && hwc_display_[display]->GetDisplayClass() == DISPLAY_CLASS_BUILTIN) {
    error = INT32(hwc_display_[display]->GetPanelMaxBrightness(max_brightness_level));
    if (error) {
      DLOGE("Failed to get the panel max brightness, display %u error %d", display, error);
    }
  }

  return error;
}

int HWCSession::SetCameraSmoothInfo(CameraSmoothOp op, int32_t fps) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyCameraSmoothInfo(op, fps);
    }
  }

  return 0;
}

int HWCSession::NotifyResolutionChange(int32_t disp_id, Attributes &attr) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyResolutionChange(disp_id, attr);
    }
  }

  return 0;
}

int HWCSession::NotifyTUIEventDone(int disp_id, TUIEventType event_type) {
  int ret = 0;
  {
    std::lock_guard<std::mutex> guard(tui_handler_lock_);
    if (tui_event_handler_future_.valid()) {
      ret = tui_event_handler_future_.get();
    }
  }
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  AIDLDisplayType disp_type = MapDisplayId(disp_id);
  for (auto const &[id, callback] : callback_clients_) {
    if (callback) {
      callback->notifyTUIEventDone(ret, disp_type, event_type);
    }
  }
  return 0;
}

int HWCSession::RegisterCallbackClient(const std::shared_ptr<IDisplayConfigCallback> &callback,
                                       int64_t *client_handle) {
  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  callback_clients_.emplace(callback_client_id_, callback);
  *client_handle = callback_client_id_;
  callback_client_id_++;

  return 0;
}

int HWCSession::UnregisterCallbackClient(const int64_t client_handle) {
  bool removed = false;

  std::lock_guard<decltype(callbacks_lock_)> lock_guard(callbacks_lock_);
  for (auto it = callback_clients_.begin(); it != callback_clients_.end();) {
    if (it->first == client_handle) {
      it = callback_clients_.erase(it);
      removed = true;
    } else {
      it++;
    }
  }

  return removed ? 0 : -EINVAL;
}

int HWCSession::SetDisplayDppsAdROI(uint32_t display_id, uint32_t h_start, uint32_t h_end,
                                    uint32_t v_start, uint32_t v_end, uint32_t factor_in,
                                    uint32_t factor_out) {
  return INT(CallDisplayFunction(display_id, &HWCDisplay::SetDisplayDppsAdROI, h_start, h_end,
                                 v_start, v_end, factor_in, factor_out));
}

int32_t HWCSession::CWB::PostBuffer(std::shared_ptr<IDisplayConfigCallback> callback,
                                    const CwbConfig &cwb_config, const native_handle_t *buffer,
                                    Display display_type, int dpy_index) {
  HWC3::Error error = HWC3::Error::None;
  auto &session_map = display_cwb_session_map_[dpy_index];
  std::shared_ptr<QueueNode> node = nullptr;
  uint64_t node_handle_id = 0;
  void *hdl = const_cast<native_handle_t *>(buffer);
  auto err =
      gralloc::GetMetaDataValue(hdl, (int64_t)StandardMetadataType::BUFFER_ID, &node_handle_id);
  if (err != gralloc::Error::NONE || node_handle_id == 0) {
    error = HWC3::Error::BadLayer;
    DLOGE("Buffer handle id retrieval failed!");
  }

  if (error == HWC3::Error::None) {
    node = std::make_shared<QueueNode>(callback, cwb_config, buffer, display_type, node_handle_id);
    if (node) {
      // Keep CWB request handling related resources in a requested display context.
      std::unique_lock<std::mutex> lock(session_map.lock);

      // Iterate over the queue to avoid duplicate node of same buffer, because that
      // buffer is already present in queue.
      for (auto &qnode : session_map.queue) {
        if (qnode->handle_id == node_handle_id) {
          error = HWC3::Error::BadParameter;
          DLOGW("CWB Buffer with handle id %lu is already available in Queue for processing!",
                node_handle_id);
          break;
        }
      }

      // Ensure that async task runs only until all queued CWB requests have been fulfilled.
      // If cwb queue is empty, async task has not either started or async task has finished
      // processing previously queued cwb requests. Start new async task on such a case as
      // currently running async task will automatically desolve without processing more requests.
      if (error == HWC3::Error::None) {
        session_map.queue.push_back(node);
      }
    } else {
      error = HWC3::Error::BadParameter;
      DLOGE("Unable to allocate node for CWB request(handle id: %lu)!", node_handle_id);
    }
  }

  if (error == HWC3::Error::None) {
    SCOPE_LOCK(hwc_session_->locker_[dpy_index]);
    // Get display instance using display type.
    HWCDisplay *hwc_display = hwc_session_->hwc_display_[dpy_index];
    if (!hwc_display) {
      error = HWC3::Error::BadDisplay;
    } else {
      // Send CWB request to CWB Manager
      error = hwc_display->SetReadbackBuffer(buffer, nullptr, cwb_config, kCWBClientExternal);
    }
  }

  if (error == HWC3::Error::None) {
    DLOGV_IF(kTagCwb, "Successfully configured CWB buffer(handle id: %lu).", node_handle_id);
  } else {
    // Need to close and delete the cloned native handle on CWB request rejection/failure and
    // if node is created and pushed, then need to remove from queue.
    native_handle_close(buffer);
    native_handle_delete(const_cast<native_handle_t *>(buffer));
    std::unique_lock<std::mutex> lock(session_map.lock);
    // If current node is pushed in the queue, then need to remove it again on error.
    if (node && node == session_map.queue.back()) {
      session_map.queue.pop_back();
    }
    return -1;
  }

  std::unique_lock<std::mutex> lock(session_map.lock);
  if (!session_map.async_thread_running && !session_map.queue.empty()) {
    session_map.async_thread_running = true;
    // No need to do future.get() here for previously running async task. Async method will
    // guarantee to exit after cwb for all queued requests is indeed complete i.e. the respective
    // fences have signaled and client is notified through registered callbacks. This will make
    // sure that the new async task does not concurrently work with previous task. Let async
    // running thread dissolve on its own.
    // Check, If thread is not running, then need to re-execute the async thread.
    session_map.future = std::async(HWCSession::CWB::AsyncTaskToProcessCWBStatus, this, dpy_index);
  }

  if (node) {
    node->request_completed = true;
  }

  return 0;
}

int HWCSession::CWB::OnCWBDone(int dpy_index, int32_t status, uint64_t handle_id) {
  auto &session_map = display_cwb_session_map_[dpy_index];

  {
    std::unique_lock<std::mutex> lock(session_map.lock);
    // No need to notify to the client, if there is no any pending CWB request in queue.
    if (session_map.queue.empty()) {
      return -1;
    }

    for (auto &node : session_map.queue) {
      if (node->notified_status == kCwbNotifiedNone) {
        // Need to wait for other notification, when notified handle id does not match with
        // available first non-notified node buffer handle id in queue.
        if (node->handle_id == handle_id) {
          node->notified_status = (status) ? kCwbNotifiedFailure : kCwbNotifiedSuccess;
          session_map.cv.notify_one();
          return 0;
        } else {
          // Continue to check on not matching handle_id, to update the status of any matching
          // node, because if notification for particular handle_id skip, then it will not update
          // again and notification thread will wait for skipped node forever.
          continue;
        }
      }
    }
  }

  return -1;
}

void HWCSession::CWB::AsyncTaskToProcessCWBStatus(CWB *cwb, int dpy_index) {
  cwb->ProcessCWBStatus(dpy_index);
}

void HWCSession::CWB::ProcessCWBStatus(int dpy_index) {
  auto &session_map = display_cwb_session_map_[dpy_index];
  while (true) {
    std::shared_ptr<QueueNode> cwb_node = nullptr;
    {
      std::unique_lock<std::mutex> lock(session_map.lock);
      // Exit thread in case of no pending CWB request in queue.
      if (session_map.queue.empty()) {
        // Update thread exiting status.
        session_map.async_thread_running = false;
        break;
      }

      cwb_node = session_map.queue.front();
      if (!cwb_node->request_completed) {
        // Need to continue to recheck until node specific client call completes.
        continue;
      } else if (cwb_node->notified_status == kCwbNotifiedNone) {
        // Wait for the signal for availability of CWB notified node.
        session_map.cv.wait(lock);
        if (cwb_node->notified_status == kCwbNotifiedNone) {
          // If any other node notified before front node, then need to continue to wait
          // for front node, such that further client notification will be done in sequential
          // manner.
          DLOGW("CWB request is notified out of sequence.");
          continue;
        }
      }
      session_map.queue.pop_front();
    }

    // Notify to client, when notification is received successfully for expected input buffer.
    NotifyCWBStatus(cwb_node->notified_status, cwb_node);
  }
  DLOGI("CWB queue is empty. Display: %d", dpy_index);
}

void HWCSession::CWB::NotifyCWBStatus(int status, std::shared_ptr<QueueNode> cwb_node) {
  // Notify client about buffer status and erase the node from pending request queue.
  std::shared_ptr<IDisplayConfigCallback> callback = cwb_node->callback;
  if (callback) {
    DLOGI("Notify the client about buffer status %d.", status);
    callback->notifyCWBBufferDone(status, ::android::dupToAidl(cwb_node->buffer));
  }

  native_handle_close(cwb_node->buffer);
  native_handle_delete(const_cast<native_handle_t *>(cwb_node->buffer));
}

int HWCSession::NotifyCwbDone(int dpy_index, int32_t status, uint64_t handle_id) {
  return cwb_.OnCWBDone(dpy_index, status, handle_id);
}

int HWCSession::GetSupportedDisplayRefreshRates(int disp_id,
                                                std::vector<uint32_t> *supported_refresh_rates) {
  int disp_idx = GetDisplayIndex(disp_id);
  if (disp_idx == -1) {
    DLOGE("Invalid display = %d", disp_id);
    return -EINVAL;
  }

  SCOPE_LOCK(locker_[disp_idx]);

  if (hwc_display_[disp_idx]) {
    return hwc_display_[disp_idx]->GetSupportedDisplayRefreshRates(supported_refresh_rates);
  }
  return -EINVAL;
}
}  // namespace sdm
