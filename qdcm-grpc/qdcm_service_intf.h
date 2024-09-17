/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QDCM_SERVICE_INTF_H__
#define __QDCM_SERVICE_INTF_H__

#include <qdcm_service.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include <functional>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

#define QDCM_GRPC_SERVER_ADDR "0.0.0.0:50391"

class QdcmServiceIntf {
 public:
  virtual int Init(std::function<int(const void *, void *)> notifyCallback) = 0;
  virtual void DeInit() = 0;
};

class QdcmServiceFactory {
 public:
  static QdcmServiceIntf *GetQdcmServiceInstance();
};

#endif /* __QDCM_SERVICE_INTF_H__ */