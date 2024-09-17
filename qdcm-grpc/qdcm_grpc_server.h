/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <qdcm_service.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <debug_handler.h>

#include <functional>
#include <string>

class QdcmDisplayApiSvc final : public QdcmDisplayApi::Service {
 public:
  QdcmDisplayApiSvc(std::function<int(const void *, void *)> callback, std::string server_addr);
  void Init();
  void Deinit();
  void RunServer();  // should be called in its own thread
  grpc::Status Dispatch(grpc::ServerContext *sc, const QdcmPacket *in, QdcmPacket *out) override;

 private:
  std::unique_ptr<grpc::Server> server_ = nullptr;
  std::function<int(const void *, void *)> callback_;
  std::string server_addr_;
};