/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <thread>
#include <grpcpp/grpcpp.h>

#include "qdcm_service_impl.h"

static QdcmServiceIntfImpl qdcm_service;

QdcmServiceIntf *QdcmServiceFactory::GetQdcmServiceInstance() {
  return &qdcm_service;
}

#undef __CLASS__
#define __CLASS__ "QdcmServiceIntfImpl"

int QdcmServiceIntfImpl::Init(std::function<int(const void *, void *)> notifyCallback) {
  if (init_done_) {
    return 0;
  }

  grpc_svc_ = std::make_shared<QdcmDisplayApiSvc>(notifyCallback, server_addr_);
  grpc_svc_->Init();
  std::thread thr(&QdcmServiceIntfImpl::RunService, this);
  thr.detach();

  init_done_ = true;
  return 0;
}

void QdcmServiceIntfImpl::DeInit() {
  if (grpc_svc_) {
    grpc_svc_->Deinit();
    grpc_svc_ = nullptr;
  }

  init_done_ = false;
}

void QdcmServiceIntfImpl::RunService() {
  if (grpc_svc_) {
    grpc_svc_->RunServer();
  }
}