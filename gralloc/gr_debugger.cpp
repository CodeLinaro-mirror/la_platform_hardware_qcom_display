/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <utils/constants.h>
#include <cutils/properties.h>
#include <display_properties.h>

#include "gr_debugger.h"

namespace gralloc {
GrDebugHandler GrDebugHandler::debug_handler_;
GrDebugHandler::GrDebugHandler() {
  //DebugHandler::Set(GrDebugHandler::Get());
}

int GrDebugHandler::GetProperty(const char *property_name, int *value) {
  char property[PROPERTY_VALUE_MAX];

  if (property_get(property_name, property, NULL) > 0) {
    *value = atoi(property);
    return kErrorNone;
  }

  return kErrorNotSupported;
}

int GrDebugHandler::GetProperty(const char *property_name, char *value) {
  if (property_get(property_name, value, NULL) > 0) {
    return kErrorNone;
  }

  return kErrorNotSupported;
}

}  // namespace gralloc
