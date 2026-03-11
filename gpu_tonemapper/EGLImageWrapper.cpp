/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "EGLImageWrapper.h"
#include <cutils/native_handle.h>
#include <QtiGralloc.h>
#include <QtiGrallocPriv.h>
#include <ui/GraphicBuffer.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <utility>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include "debug_callback_intf.h"

using aidl::android::hardware::graphics::common::StandardMetadataType;
using ::sdm::DebugCallbackIntf;
using std::map;
using std::pair;
using std::string;
using private_handle_t = qtigralloc::private_handle_t;

static string pidString = std::to_string(getpid());

//-----------------------------------------------------------------------------
static string get_ion_buff_str(int buff_fd)
//-----------------------------------------------------------------------------
{
  string retStr = {};
  if (buff_fd >= 0) {
    struct stat stat1;
    fstat(buff_fd, &stat1);
    retStr = std::to_string(stat1.st_ino);
  }

  return retStr;
}

//-----------------------------------------------------------------------------
void EGLImageWrapper::DeleteEGLImageCallback::operator()(int& buffInt, EGLImageBuffer*& eglImage)
//-----------------------------------------------------------------------------
{
  if (eglImage != 0) {
    delete eglImage;
  }

  if (!mapClearPending) {
    for (auto it = buffStrbuffIntMapPtr->begin(); it != buffStrbuffIntMapPtr->end(); it++) {
      if (it->second == buffInt /* counter */) {
        buffStrbuffIntMapPtr->erase(it);
        return;
      }
    }
  }
}

//-----------------------------------------------------------------------------
EGLImageWrapper::EGLImageWrapper()
//-----------------------------------------------------------------------------
{
  Init();
}

//-----------------------------------------------------------------------------
EGLImageWrapper::~EGLImageWrapper()
//-----------------------------------------------------------------------------
{
  Deinit();
}

//-----------------------------------------------------------------------------
void EGLImageWrapper::Init()
//-----------------------------------------------------------------------------
{
  eglImageBufferCache = new android::LruCache<int, EGLImageBuffer*>(32);
  callback = new DeleteEGLImageCallback(&buffStrbuffIntMap);
  eglImageBufferCache->setOnEntryRemovedListener(callback);

  const std::string snapalloc_lib_name = "vendor.qti.hardware.display.snapalloc-impl.so";
  void *snap_impl_lib_ = ::dlopen(snapalloc_lib_name.c_str(), RTLD_NOW);
  if (!snap_impl_lib_) {
    ALOGE("Dlopen error for snapalloc impl: %s", dlerror());
  }

  std::shared_ptr<ISnapMapper> (*LINK_FETCH_ISnapMapper)(DebugCallbackIntf *) = nullptr;
  *reinterpret_cast<void **>(&LINK_FETCH_ISnapMapper) =
      ::dlsym(snap_impl_lib_, "FETCH_ISnapMapper");
  if (LINK_FETCH_ISnapMapper) {
    snapmapper_ = LINK_FETCH_ISnapMapper(nullptr);
  } else {
    ALOGE("Failed to get snapalloc instance");
  }
}

//-----------------------------------------------------------------------------
void EGLImageWrapper::Deinit()
//-----------------------------------------------------------------------------
{
  if (eglImageBufferCache != 0) {
    if (callback != 0) {
      callback->mapClearPending = true;
    }
    eglImageBufferCache->clear();
    delete eglImageBufferCache;
    eglImageBufferCache = 0;
    buffStrbuffIntMap.clear();
  }

  if (callback != 0) {
    delete callback;
    callback = 0;
  }

}

static native_handle_t *CNativeHandleFromSnapHandle(const SnapHandle *snap_handle,
                                                    bool pass_fd_ownership) {
  if (!snap_handle)
    return nullptr;

  native_handle_t *native_handle =
      native_handle_create(snap_handle->num_fds, snap_handle->num_ints);

  if (!native_handle)
    return nullptr;

  for (size_t i = 0; i < snap_handle->num_fds; i++) {
    int fd = snap_handle->buffer_data[i];
    native_handle->data[i] = pass_fd_ownership ? fd : fcntl(fd, F_DUPFD_CLOEXEC, 0);
  }

  memcpy((native_handle->data + native_handle->numFds),
         (snap_handle->buffer_data + snap_handle->num_fds), snap_handle->num_ints * sizeof(int));

  return native_handle;
}

//-----------------------------------------------------------------------------
static EGLImageBuffer *L_wrap(const SnapHandle *snap_hnd, std::shared_ptr<ISnapMapper> snapmapper)
//-----------------------------------------------------------------------------
{
  EGLImageBuffer* result = 0;

  uint32_t stride = 0;
  uint32_t format = 0;

  if (snapmapper->GetMetadata(*snap_hnd, PIXEL_FORMAT_REQUESTED, &format) != SnapError::NONE) {
    ALOGE("Failed to get format from snapalloc");
    return nullptr;
  }

  if (snapmapper->GetMetadata(*snap_hnd, STRIDE, &stride) != SnapError::NONE) {
    ALOGE("Failed to get stride from snapalloc");
    return nullptr;
  }

  uint64_t protected_content;
  if (snapmapper->GetMetadata(*snap_hnd, PROTECTED_CONTENT, &protected_content) !=
      SnapError::NONE) {
    ALOGE("Failed to get protected flag");
    return nullptr;
  }

  uint32_t height = 0;
  if (snapmapper->GetMetadata(*snap_hnd, HEIGHT, &height) != SnapError::NONE) {
    ALOGE("Failed to get height from snapalloc");
    return nullptr;
  }

  uint32_t width = 0;
  if (snapmapper->GetMetadata(*snap_hnd, WIDTH, &width) != SnapError::NONE) {
    ALOGE("Failed to get width from snapalloc");
    return nullptr;
  }

  native_handle_t *handle = CNativeHandleFromSnapHandle(snap_hnd, false);
  if (handle == nullptr) {
    ALOGE("Unable to create native handle from Snap handle");
    return nullptr;
  }

  int flags = android::GraphicBuffer::USAGE_HW_TEXTURE |
              android::GraphicBuffer::USAGE_SW_READ_NEVER |
              android::GraphicBuffer::USAGE_SW_WRITE_NEVER;

  if (protected_content) {
    flags |= android::GraphicBuffer::USAGE_PROTECTED;
  }

  android::sp<android::GraphicBuffer> graphicBuffer = new android::GraphicBuffer(
      handle, android::GraphicBuffer::CLONE_HANDLE, width, height, format, 1, //Layer Count
      static_cast<uint64_t>(flags), stride);

  native_handle_close(handle);
  native_handle_delete(handle);

  result = new EGLImageBuffer(graphicBuffer);

  return result;
}

//-----------------------------------------------------------------------------
EGLImageBuffer *EGLImageWrapper::wrap(const SnapHandle *snap_hnd)
//-----------------------------------------------------------------------------
{
  uint32_t fd = 0;

  snapmapper_->GetMetadata(*snap_hnd, FD, &fd);
  string buffStr = get_ion_buff_str(fd);
  EGLImageBuffer* eglImage = nullptr;
  if (!buffStr.empty()) {
    auto it = buffStrbuffIntMap.find(buffStr);
    if (it != buffStrbuffIntMap.end()) {
      eglImage = eglImageBufferCache->get(it->second);
    } else {
      eglImage = L_wrap(snap_hnd, snapmapper_);
      buffStrbuffIntMap.insert(pair<string, int>(buffStr, buffInt));
      eglImageBufferCache->put(buffInt, eglImage);
      buffInt++;
    }
  } else {
    ALOGE("Could not provide an eglImage for fd = %d, EGLImageWrapper = %p", fd, this);
  }

  return eglImage;
}
