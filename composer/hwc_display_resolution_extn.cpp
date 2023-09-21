/*
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
  SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <string>
#include <vector>
#include <tinyxml2.h>
#include <fstream>

#include "hwc_display_resolution_extn.h"

#define __CLASS__ "HWCDisplayResolutionExtn"
#define HWC_DISPLAY_RESOLUTION_EXTN_FILE "/vendor/etc/display/hwc_display_resolution_extn.xml"

using tinyxml2::XMLElement;
using tinyxml2::XMLDocument;
using tinyxml2::XML_SUCCESS;
using namespace::std;

namespace sdm {

DisplayError HWCDisplayResolutionExtn::GetExtendedDisplayResolutions(uint32_t panel_width,
                                                                     uint32_t panel_height,
                                          vector<pair<uint32_t, uint32_t>> *extended_disp_res) {
  DisplayError error = kErrorNotSupported;
  const char *soc_name = GetSocName();

  if((soc_name == NULL) || (soc_name[0] == '\0')) {
    return error;
  }

  string xml_path = string(HWC_DISPLAY_RESOLUTION_EXTN_FILE);
  XMLDocument document;

  if (document.LoadFile(xml_path.c_str()) != XML_SUCCESS) {
    DLOGE("TinyXML2 could not load file: %s", xml_path.c_str());
    return error;
  }

  XMLElement* target_node = document.RootElement();
  if (target_node == nullptr) {
    DLOGE("No target configuration specified");
    return error;
  }

  bool configuration_found = false;
  while (target_node != nullptr) {
    const char *target_name =  target_node->Attribute("name");

    if (target_name == nullptr) {
      continue;
    }
    if (!strcmp(target_name, soc_name)) {
      XMLElement* panel_res_node = target_node->FirstChildElement("PanelResolution");
      while (panel_res_node != nullptr) {
        const char *width = panel_res_node->Attribute("width");
        const char *height = panel_res_node->Attribute("height");
        if (width == nullptr || height == nullptr) {
          continue;
        }
        const int p_width = std::atoi(width);
        const int p_height = std::atoi(height);

        if (p_width == panel_width && p_height == panel_height) {
          XMLElement* scaling_factor_node = panel_res_node->FirstChildElement("ScalingFactor");
          while (scaling_factor_node != nullptr) {
            const char *x = scaling_factor_node->Attribute("x");
            const char *y = scaling_factor_node->Attribute("y");
            if (x == nullptr || y == nullptr) {
              continue;
            }
            const float x_factor = std::atof(x);
            const float y_factor = std::atof(y);

            float res_x = panel_width / x_factor;
            float res_y = panel_height / y_factor;
            if ((floorf(res_x) == res_x) && (floorf(res_y) == res_y) &&
                (UINT32(res_x) % 2 == 0) && (UINT32(res_y) % 2 == 0)) {
              extended_disp_res->push_back(std::make_pair(UINT32(res_x), UINT32(res_y)));
            } else {
              DLOGI("scaling factor: %f x %f is invalid", x_factor, y_factor);
            }

            scaling_factor_node = scaling_factor_node->NextSiblingElement("ScalingFactor");
          }
          configuration_found = true;
          break;
        }
        panel_res_node = panel_res_node->NextSiblingElement("PanelResolution");
      }
    }
    if (configuration_found) {
      error = kErrorNone;
      break;
    }
    target_node = target_node->NextSiblingElement("Target");
  }

  return error;
}

}
