/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __GR_DEBUGGER_H__
#define __GR_DEBUGGER_H__

#include <debug_handler.h>

namespace gralloc {
using display::DebugHandler;
enum GrallocError {
  kErrorNone,          //!< Call executed successfully.,
  kErrorNotSupported,  //!< Requested operation is not supported.
};
class GrDebugHandler : public DebugHandler {
 public:
  GrDebugHandler();
  static inline DebugHandler *Get() { return &debug_handler_; }
  virtual void Error(const char *fmt, ...) __attribute__((format(printf, 2, 3))){};
  virtual void Warning(const char *fmt, ...) __attribute__((format(printf, 2, 3))){};
  virtual void Info(const char *fmt, ...) __attribute__((format(printf, 2, 3))){};
  virtual void Debug(const char *fmt, ...) __attribute__((format(printf, 2, 3))){};
  virtual void Verbose(const char *fmt, ...) __attribute__((format(printf, 2, 3))){};
  virtual void BeginTrace(const char *class_name, const char *function_name,
                          const char *custom_string){};
  virtual void EndTrace(){};
  virtual int GetProperty(const char *property_name, int *value);
  virtual int GetProperty(const char *property_name, char *value);

 private:
  static GrDebugHandler debug_handler_;
};
}  // namespace gralloc
#endif  // __GR_DEBUGGER_H__