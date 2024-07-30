/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __QDCM_SERVICE_IMPL_H__
#define __QDCM_SERVICE_IMPL_H__

#include <memory>
#include <map>
#include <functional>

#include "qdcm_service_intf.h"
#include "qdcm_grpc_server.h"

class QdcmServiceIntfImpl : public QdcmServiceIntf {
 public:
  int Init(std::function<int(const void *, void *)> notifyCallback);
  void DeInit();

 private:
  void RunService();

  bool init_done_ = false;
  std::shared_ptr<QdcmDisplayApiSvc> grpc_svc_ = nullptr;
  std::string server_addr_ = QDCM_GRPC_SERVER_ADDR;
};

#endif /* __QDCM_SERVICE_IMPL_H__ */