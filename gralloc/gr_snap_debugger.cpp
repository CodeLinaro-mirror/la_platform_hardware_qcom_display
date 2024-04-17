/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cutils/properties.h>

#include "gr_snap_debugger.h"

namespace gralloc {

void GrallocSnapDebugger::Log(DebugLogType type, const char *log_tag, const char *fmt,
                              std::va_list &args) {
  auto log_type = log_type_map_[type];
  __android_log_vprint(log_type, log_tag, fmt, args);
}

int GrallocSnapDebugger::GetProperty(const char *property_name, int *value) {
  char property[PROPERTY_VALUE_MAX];

  if (property_get(property_name, property, NULL) > 0) {
    *value = atoi(property);
    return sdm::kErrorNone;
  }

  return sdm::kErrorNotSupported;
}

int GrallocSnapDebugger::GetProperty(const char *property_name, char *value) {
  if (property_get(property_name, value, NULL) > 0) {
    return sdm::kErrorNone;
  }

  return sdm::kErrorNotSupported;
}

void GrallocSnapDebugger::BeginTrace(const char *class_name, const char *function_name,
                                     const char *custom_string) {
  if (atrace_is_tag_enabled(ATRACE_TAG)) {
    char name[PATH_MAX] = {0};
    snprintf(name, sizeof(name), "%s::%s::%s", class_name, function_name, custom_string);
    atrace_begin(ATRACE_TAG, name);
  }
}

void GrallocSnapDebugger::EndTrace() {
  atrace_end(ATRACE_TAG);
}

void GrallocSnapDebugger::ATrace(const char *custom_string, const int bit) {
  ATRACE_INT(custom_string, bit);
}

}  // namespace gralloc