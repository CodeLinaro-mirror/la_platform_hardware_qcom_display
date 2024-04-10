/*
* Copyright (c) 2017 - 2018, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of The Linux Foundation nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
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
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#ifdef __MIN_ANDROID_VER_T__
#include <display/drm/sde_drm.h>
#else
#include <drm/sde_drm.h>
#endif

// Intentionally included after xf86 headers so that they in-turn include libdrm version of drm.h
// that doesn't use keyword "virtual" for a variable name. Not doing so leads to the kernel version
// of drm.h being included causing compilation to fail
#include <drm/msm_drm.h>
#include <algorithm>
#include <iterator>

#include "drm_master.h"

#define __CLASS__ "DRMMaster"

using std::mutex;
using std::lock_guard;
using std::begin;
using std::copy;
using std::end;
using std::fill;

namespace drm_utils {

std::map<uint32_t, DRMMaster*> DRMMaster::s_instance;
mutex DRMMaster::s_lock;

int DRMMaster::GetInstance(DRMMaster **master, uint32_t card) {
  lock_guard<mutex> obj(s_lock);

  auto iter = s_instance.find(card);
  if (iter == s_instance.end()) {
    DRMMaster *new_master = new DRMMaster();
    if (new_master->Init(card) < 0) {
      delete new_master;
      return -ENODEV;
    }
    s_instance[card] = new_master;
    *master = new_master;
  } else {
    *master = iter->second;
  }

  return 0;
}

void DRMMaster::DestroyInstance(uint32_t card) {
  lock_guard<mutex> obj(s_lock);

  auto iter = s_instance.find(card);
  if (iter != s_instance.end()) {
    delete iter->second;
    s_instance.erase(iter);
  }
}

/*
 * if DRM card leasing is enabled
 *    card 0 will not be able to open because,all its resources are lease to other cards
 *    We will open the instance of the card which will have the name as msm_drm
 *
 * if DRM card leasing feature not enabled
 *    card0 and card1 will be opened
 *
 * */

int DRMMaster::Init(uint32_t card) {
  if (card == 0) {
    dev_fd_ = drmOpen("msm_drm", nullptr);
  } else {
    drmVersionPtr ver;
    int fd;
    bool first_match = false;
    int i;

    for (i = 0; i < 16; i++) {
      snprintf(path_, sizeof(path_), "/dev/dri/card%d", i);
      fd = open(path_, O_RDWR | O_CLOEXEC, 0);
      if (fd < 0) {
        continue;
      }

      ver = drmGetVersion(fd);
      if (ver) {
        bool match = !strcmp(ver->name, "msm_drm");
        drmFreeVersion(ver);
        if (match) {
          if (!first_match) {
            first_match = true;
            close(fd);
          } else {
            dev_fd_ = fd;
            card_ = card;
          }
           /* Revisit the exit logic for more than 2 DPUs*/
          if (first_match)
            continue;
          else
            break;
        }
      }

      close(fd);
    }
  }

  if (dev_fd_ < 0) {
    DRM_LOGE("drmOpen failed with error %d", dev_fd_);
    return -ENODEV;
  }

  return 0;
}

DRMMaster::~DRMMaster() {
  drmClose(dev_fd_);
  dev_fd_ = -1;
}

int DRMMaster::CreateFbId(const DRMBuffer &drm_buffer, uint32_t *fb_id) {
  uint32_t gem_handle = 0;
  int ret = drmPrimeFDToHandle(dev_fd_, drm_buffer.fd, &gem_handle);
  if (ret) {
    DRM_LOGE("drmPrimeFDToHandle failed with error %d", ret);
    return ret;
  }

  struct drm_mode_fb_cmd2 cmd2 {};
  cmd2.width = drm_buffer.width;
  cmd2.height = drm_buffer.height;
  cmd2.pixel_format = drm_buffer.drm_format;
  cmd2.flags = DRM_MODE_FB_MODIFIERS;
  fill(begin(cmd2.handles), begin(cmd2.handles) + drm_buffer.num_planes, gem_handle);
  copy(begin(drm_buffer.stride), end(drm_buffer.stride), begin(cmd2.pitches));
  copy(begin(drm_buffer.offset), end(drm_buffer.offset), begin(cmd2.offsets));
  fill(begin(cmd2.modifier), begin(cmd2.modifier) + drm_buffer.num_planes,
       drm_buffer.drm_format_modifier);

  if ((ret = drmIoctl(dev_fd_, DRM_IOCTL_MODE_ADDFB2, &cmd2))) {
    DRM_LOGE("DRM_IOCTL_MODE_ADDFB2 failed with error %d", ret);
  } else {
    *fb_id = cmd2.fb_id;
  }

  struct drm_gem_close gem_close = {};
  gem_close.handle = gem_handle;
  int ret1 = drmIoctl(dev_fd_, DRM_IOCTL_GEM_CLOSE, &gem_close);
  if (ret1) {
    DRM_LOGE("drmIoctl::DRM_IOCTL_GEM_CLOSE failed with error %d", ret1);
    return ret1;
  }

  return ret;
}

int DRMMaster::RemoveFbId(uint32_t fb_id) {
  int ret = 0;
#ifdef DRM_IOCTL_MSM_RMFB2
  ret = drmIoctl(dev_fd_, DRM_IOCTL_MSM_RMFB2, &fb_id);
  if (ret) {
    DRM_LOGE("drmIoctl::DRM_IOCTL_MSM_RMFB2 failed for fb_id %d with error %d", fb_id, errno);
  }
#else
  ret = drmModeRmFB(dev_fd_, fb_id);
  if (ret) {
    DRM_LOGE("drmModeRmFB failed for fb_id %d with error %d", fb_id, ret);
  }
#endif
  return ret;
}

bool DRMMaster::IsRmFbRefCounted() {
#ifdef DRM_IOCTL_MSM_RMFB2
  return true;
#endif
  return false;
}

void DRMMaster::CreateEventHandle(int *fd) {
  if (card_ == 0) {
    *fd = drmOpen("msm_drm", nullptr);
  } else {
    *fd = open(path_, O_RDWR | O_CLOEXEC, 0);
  }
}

}  // namespace drm_utils
