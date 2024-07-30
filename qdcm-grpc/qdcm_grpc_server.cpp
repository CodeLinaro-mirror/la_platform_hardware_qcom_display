/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <qdcm_grpc_server.h>

#define __CLASS__ "QdcmDisplayApiSvc"

QdcmDisplayApiSvc::QdcmDisplayApiSvc(std::function<int(const void *, void *)> callback,
                                     std::string server_addr)
    : callback_(callback), server_addr_(server_addr) {}

void QdcmDisplayApiSvc::Deinit() {
  if (server_) {
    server_->Shutdown();
    server_.reset();
    server_ = nullptr;
  }
}

void QdcmDisplayApiSvc::Init() {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_addr_, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(this);
  // Finally assemble the server.
  server_ = builder.BuildAndStart();
  if (!server_) {
    DLOGE("Failed to start GRPC server...!");
    return;
  }

  DLOGI("QDCM Server listening on: %s", server_addr_.c_str());
}

// should be called in its own thread
void QdcmDisplayApiSvc::RunServer() {
  if (server_) {
    server_->Wait();
  }
}

grpc::Status QdcmDisplayApiSvc::Dispatch(grpc::ServerContext *sc, const QdcmPacket *in,
                                          QdcmPacket *out) {
  int err = 0;
  err = callback_(in, out);
  if (err) {
    return grpc::Status::CANCELLED;
  }

  return grpc::Status::OK;
}
