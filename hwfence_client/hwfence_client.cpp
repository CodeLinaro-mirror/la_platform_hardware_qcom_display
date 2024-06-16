/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <unistd.h>
#include <synx_header.h>
#include <linux/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/types.h>
#include <debug_handler.h>
#include "hwfence_client.h"

#define __CLASS__ "HwFenceClient"

namespace hwfence {

int HwFenceClient::Init(struct synx_init_data *data) {
  int32_t result;
  int32_t num = 0;
  struct synx_private_ioctl_arg synx_ioctl;
  char *deviceName = "/dev/synx_device";
  int32_t synx_fd = open(deviceName, O_RDWR);
  struct synx_initialize_v2 *info = new struct synx_initialize_v2;

  if (!info) {
    DLOGE("Failed to allocate info!");
    return -1;
  }

  info->id = data->id;
  info->flags = data->flags;
  DLOGI("hw fence client id :%d", info->id);

  std::memset(&synx_ioctl, 0, sizeof(synx_ioctl));
  synx_ioctl.id = SYNX_INITIALIZE;
  synx_ioctl.size = sizeof(struct synx_initialize_v2);
  synx_ioctl.ioctl_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info));

  result = ioctl(synx_fd, SYNX_PRIVATE_IOCTL_CMD, &synx_ioctl);
  if (result < 0) {
    DLOGE("SYNX_PRIVATE_IOCTL_CMD ioctl failed with error %d", result);
    delete info;
    return result;
  }

  delete info;
  return synx_fd;
}

int HwFenceClient::DeInit(int32_t fd) {
  int32_t status;

  if (fd < 0) {
    DLOGE("Invalid fd!");
    return fd;
  }

  status = fcntl(fd, F_GETFD, 0);
  if (status < 0) {
    DLOGE("fcntl F_GETFD Failed for fd %d", fd);
    return status;
  }

  return close(fd);
}

int HwFenceClient::SynxCreate(int32_t fd, struct synx_create_data *data) {
  int32_t result;
  struct synx_private_ioctl_arg synx_ioctl;
  struct synx_create_v2 *info = new struct synx_create_v2;

  if (!info) {
    DLOGE("Failed to allocate synx_create_v2!");
    return -1;
  }

  info->flags = data->flags;

  std::memset(&synx_ioctl, 0, sizeof(synx_ioctl));
  synx_ioctl.id = SYNX_CREATE;
  synx_ioctl.size = sizeof(struct synx_create_v2);
  synx_ioctl.ioctl_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info));

  result = ioctl(fd, SYNX_PRIVATE_IOCTL_CMD, &synx_ioctl);
  if (result < 0) {
    DLOGE("ioctl SYNX_PRIVATE_IOCTL_CMD Failed for fd %d error %d", fd, result);
    delete info;
    return result;
  }

  data->synx_obj = ((struct synx_create_v2 *)synx_ioctl.ioctl_ptr)->synx_obj;
  DLOGV("created synx obj:%d flags:%x", data->synx_obj, info->flags);

  delete info;
  return result;
}

int HwFenceClient::SynxRelease(int32_t fd, uint32_t synx_obj) {
  int32_t result;
  struct synx_private_ioctl_arg synx_ioctl;
  struct synx_info *info = new struct synx_info;

  if (!info) {
    DLOGE("Failed to allocate synx_info!");
    return -1;
  }

  DLOGV("Release synx obj:%d", synx_obj);
  info->synx_obj = synx_obj;

  std::memset(&synx_ioctl, 0, sizeof(synx_ioctl));
  synx_ioctl.id = SYNX_RELEASE;
  synx_ioctl.size = sizeof(struct synx_info);
  synx_ioctl.ioctl_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info));

  result = ioctl(fd, SYNX_PRIVATE_IOCTL_CMD, &synx_ioctl);
  if (result < 0) {
    DLOGE("SYNX_PRIVATE_IOCTL_CMD ioctl faile with error %d", result);
  }

  delete info;
  return result;
}

int HwFenceClient::GetSynxStatus(int32_t fd, uint32_t synx_obj) {
  int32_t result, synx_state;
  struct synx_private_ioctl_arg synx_ioctl;
  struct synx_signal_v2 *info = new struct synx_signal_v2;

  if (!info) {
    DLOGE("Invalid synx_signal_v2!");
    return -1;
  }
  info->synx_obj = synx_obj;

  std::memset(&synx_ioctl, 0, sizeof(synx_ioctl));
  synx_ioctl.id = SYNX_GETSTATUS;
  synx_ioctl.size = sizeof(struct synx_signal_v2);
  synx_ioctl.ioctl_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info));

  result = ioctl(fd, SYNX_PRIVATE_IOCTL_CMD, &synx_ioctl);
  if (result < 0) {
    DLOGE("SYNX_PRIVATE_IOCTL_CMD ioctl failed with error %d", result);
    delete info;
    return result;
  }

  synx_state = ((struct synx_signal_v2 *)synx_ioctl.ioctl_ptr)->synx_state;
  DLOGV("Getstatus synxobj:%d state:%d", synx_obj, synx_state);

  delete info;
  return synx_state;
}

int HwFenceClient::GetSynxFd(int32_t fd, uint32_t synx_obj) {
  int32_t result, fence_fd;
  struct synx_private_ioctl_arg synx_ioctl;
  struct synx_fence_fd *info = new struct synx_fence_fd;

  if (!info) {
    DLOGE("Invalid synx_fence_fd!");
    return -1;
  }
  info->synx_obj = synx_obj;

  std::memset(&synx_ioctl, 0, sizeof(synx_ioctl));
  synx_ioctl.id = SYNX_GETFENCE_FD;
  synx_ioctl.size = sizeof(struct synx_fence_fd);
  synx_ioctl.ioctl_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info));

  result = ioctl(fd, SYNX_PRIVATE_IOCTL_CMD, &synx_ioctl);
  if (result < 0) {
    DLOGE("SYNX_PRIVATE_IOCTL_CMD ioctl failed with error %d", result);
    delete info;
    return result;
  }
  fence_fd = ((struct synx_fence_fd *)synx_ioctl.ioctl_ptr)->fd;
  DLOGV("synxobj:%d fence_fd:%d", synx_obj, fence_fd);

  delete info;
  return fence_fd;
}

}  // namespace hwfence
