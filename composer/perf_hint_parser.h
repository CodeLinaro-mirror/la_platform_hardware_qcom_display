/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __PERF_HINT_PARSER_H__
#define __PERF_HINT_PARSER_H__

#include "hwc_common.h"
#include <unordered_map>

using std::pair;
using std::unordered_map;

namespace sdm {

class PerfHintParser {
 public:
  HWC3::Error GetPerfHintThresholds(unordered_map<int32_t, int32_t> *fps_to_threshold);
  HWC3::Error Init();

 private:
  const char *kPerfThresholdXmlPath = "/vendor/etc/display/perf_hint_threshold.xml";
  unordered_map<int32_t, int32_t> fps_to_threshold_map_;

  bool LoadPerfHintThresholdFromFile(const char *file_name);
};

}  // namespace sdm

#endif  // __PERF_HINT_PARSER_H__