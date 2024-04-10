/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "hwc_common.h"

#define __CLASS__ "HWCCommon"

namespace sdm {

std::string to_string(HWC3::Error error) {
  switch (error) {
    case HWC3::Error::None:
      return "None";
    case HWC3::Error::BadConfig:
      return "BadConfig";
    case HWC3::Error::BadDisplay:
      return "BadDisplay";
    case HWC3::Error::BadLayer:
      return "BadLayer";
    case HWC3::Error::BadParameter:
      return "BadParameter";
    case HWC3::Error::HasChanges:
      return "HasChanges";
    case HWC3::Error::NoResources:
      return "NoResources";
    case HWC3::Error::NotValidated:
      return "NotValidated";
    case HWC3::Error::Unsupported:
      return "Unsupported";
    case HWC3::Error::SeamlessNotAllowed:
      return "SeamlessNotAllowed";
    default:
      return "Unknown";
  }
}

std::string to_string(PowerMode mode) {
  switch (mode) {
    case PowerMode::OFF:
      return "OFF";
    case PowerMode::ON:
      return "ON";
    case PowerMode::DOZE:
      return "DOZE";
    case PowerMode::DOZE_SUSPEND:
      return "DOZE_SUSPEND";
    case PowerMode::ON_SUSPEND:
      return "ON_SUSPEND";
    default:
      return "Unknown";
  }
}

std::string to_string(Composition composition) {
  switch (composition) {
    case Composition::INVALID:
      return "INVALID";
    case Composition::CLIENT:
      return "CLIENT";
    case Composition::DEVICE:
      return "DEVICE";
    case Composition::SOLID_COLOR:
      return "SOLID_COLOR";
    case Composition::CURSOR:
      return "CURSOR";
    case Composition::SIDEBAND:
      return "SIDEBAND";
    case Composition::DISPLAY_DECORATION:
      return "DISPLAY_DECORATION";
    default:
      return "Unknown";
  }
}

SnapHandle *ConvertToSnapHandle(const NativeHandle &handle) {
  SnapHandle *snap_handle = snap_handle_create(handle.fds.size(), handle.ints.size());
  for (size_t i = 0; i < handle.fds.size(); ++i) {
    snap_handle->buffer_data[i] = fcntl(handle.fds[i].get(), F_DUPFD_CLOEXEC, 0);
  }
  for (size_t i = 0; i < handle.ints.size(); ++i) {
    snap_handle->buffer_data[i + handle.fds.size()] = handle.ints[i];
  }
  return snap_handle;
}

NativeHandle AIDLNativeHandleFromSnapHandle(SnapHandle *snap_buffer_handle,
                                            bool pass_fd_ownership) {
  NativeHandle aidl_native_handle;

  aidl_native_handle.fds = std::vector<ndk::ScopedFileDescriptor>(snap_buffer_handle->num_fds);
  for (size_t i = 0; i < snap_buffer_handle->num_fds; i++) {
    int fd = snap_buffer_handle->buffer_data[i];
    aidl_native_handle.fds.at(i).set(pass_fd_ownership ? fd : fcntl(fd, F_DUPFD_CLOEXEC, 0));
  }

  aidl_native_handle.ints = std::vector<int32_t>(
      snap_buffer_handle->buffer_data + snap_buffer_handle->num_fds,
      snap_buffer_handle->buffer_data + snap_buffer_handle->num_fds + snap_buffer_handle->num_ints);

  return aidl_native_handle;
}

native_handle_t *SnapHandleToLegacyHandle(const SnapHandle *snap_handle) {
  native_handle_t *handle = native_handle_create(snap_handle->num_fds, snap_handle->num_ints);
  for (size_t i = 0; i < snap_handle->num_fds; ++i) {
    handle->data[i] = fcntl(snap_handle->buffer_data[i], F_DUPFD_CLOEXEC, 0);
  }
  for (size_t i = snap_handle->num_fds; i < snap_handle->num_fds + snap_handle->num_ints; ++i) {
    handle->data[i] = snap_handle->buffer_data[i];
  }
  return handle;
}

}  // namespace sdm