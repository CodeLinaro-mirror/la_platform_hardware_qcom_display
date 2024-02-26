/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __HWFENCE_CLIENT_H__
#define __HWFENCE_CLIENT_H__

#include <stdint.h>
#include <vector>

namespace hwfence {

struct synx_init_data {
  uint32_t id;
  uint32_t flags;
};

struct synx_create_data {
  uint32_t synx_obj;
  uint32_t flags;
};

class HwFenceClient {
 public:
  int Init(struct synx_init_data *data);
  int DeInit(int32_t fd);
  int SynxCreate(int32_t fd, struct synx_create_data *data);
  int SynxRelease(int32_t fd, uint32_t synx_obj);
  int GetSynxStatus(int32_t fd, uint32_t synx_obj);
  int GetSynxFd(int32_t fd, uint32_t synx_obj);
};

}  // namespace hwfence

#endif  // __HWFENCE_CLIENT_H__
