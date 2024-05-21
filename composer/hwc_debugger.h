/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __HWC_DEBUGGER_H__
#define __HWC_DEBUGGER_H__

#define DISPLAY_ATRACE_TAG (ATRACE_TAG_GRAPHICS | ATRACE_TAG_HAL)

#include <unordered_map>
#include <log/log.h>
#include <utils/Trace.h>
#include <core/sdm_types.h>

#include "debug_callback_intf.h"

namespace sdm {

class HWCDebugger : public DebugCallbackIntf {
 public:
  void Log(DebugLogType type, const char *log_tag, const char *fmt, std::va_list &args);
  int GetProperty(const char *property_name, int *value);
  int GetProperty(const char *property_name, char *value);
  void BeginTrace(const char *class_name, const char *function_name, const char *custom_string);
  void EndTrace();
  void ATrace(const char *custom_string, const int bit);

 private:
  std::unordered_map<DebugLogType, android_LogPriority> log_type_map_ = {
      {ERROR, ANDROID_LOG_ERROR},
      {WARNING, ANDROID_LOG_WARN},
      {INFO, ANDROID_LOG_INFO},
      {DEBUG, ANDROID_LOG_DEBUG},
      {VERBOSE, ANDROID_LOG_VERBOSE}};
};

}  // namespace sdm

#endif  // __HWC_DEBUGGER_H__