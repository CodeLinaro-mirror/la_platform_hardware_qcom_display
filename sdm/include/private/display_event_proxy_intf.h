/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __DISPLAY_EVENT_PROXY_INTF_H__
#define __DISPLAY_EVENT_PROXY_INTF_H__

#include <string>
#include <private/generic_intf.h>
#include <private/generic_payload.h>
#include <private/cb_intf.h>

namespace sdm {

class DisplayInterface;

enum SdmDisplayEvents {
  kSdmOprEvent,  // OPR register value
  kSdmDisplayEventsMax = 0xff
};

enum DispEventProxyParams {
  kSetPanelOprInfoEnable,
  kDispEventProxyParamMax = 0xff,
};

enum DispEventProxyOps {
  kDispEventProxyOpsMax,
};

struct PanelOprPayload {
  uint32_t version = sizeof(PanelOprPayload);
  uint64_t flags;
  uint32_t opr_val;
};

struct PanelOprInfoParam {
  std::string name;
  bool enable;
  SdmDisplayCbInterface<PanelOprPayload> *cb_intf = nullptr;
};

using DisplayEventProxyIntf = GenericIntf<DispEventProxyParams, DispEventProxyOps, GenericPayload>;

class DispEventProxyFactIntf {
 public:
  virtual ~DispEventProxyFactIntf() {}
  virtual std::shared_ptr<DisplayEventProxyIntf> CreateDispEventProxyIntf(
      const std::string &panel_name, DisplayInterface *intf) = 0;
};

extern "C" DispEventProxyFactIntf *GetDispEventProxyFactIntf();

}  // namespace sdm
#endif  // __DISPLAY_EVENT_PROXY_INTF_H__
