/*
* Copyright (c) 2022, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted
* provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright notice, this list of
*      conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright notice, this list of
*      conditions and the following disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written
*      permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <utils/debug.h>

#include "fps_manager.h"

#define __CLASS__ "FpsManager"

namespace sdm {
FpsManager* FpsManager :: instance_ = NULL;

FpsManager::FpsManager() {
  preferred_fps_ = Debug::GetPreferredFps();

  if (preferred_fps_ > max_preferred_fps_ || preferred_fps_ < min_preferred_fps_) {
    DLOGW("Preferred fps cannot be negative or greater than 60fps");
  }
}

FpsManager* FpsManager::getInstance() {
  if (Debug::GetPreferredFps() == 0) {
    return NULL;
  }

  if (!instance_)
    instance_ = new FpsManager();

  return instance_;
}

int FpsManager::getPreferredFps() {
    return preferred_fps_;
}

int FpsManager::getPreferredConfigIndex(vector<HWDisplayAttributes> display_attributes) {
  int preferred_mode_index = -1;

  if (preferred_fps_ <= min_preferred_fps_ || preferred_fps_ > max_preferred_fps_) {
     return -1;
  }

  for (uint32_t mode_index = 0; mode_index < display_attributes.size(); mode_index++) {
    if (unsigned(preferred_fps_) != display_attributes[mode_index].fps) {
       continue;
    } else if (preferred_mode_index == -1) {
       preferred_mode_index = mode_index;
    } else if (display_attributes[preferred_mode_index].x_pixels <
                        display_attributes[mode_index].x_pixels) {
       preferred_mode_index = mode_index;
    } else if (display_attributes[preferred_mode_index].x_pixels ==
                         display_attributes[mode_index].x_pixels) {
       if ( display_attributes[preferred_mode_index].y_pixels <
                         display_attributes[mode_index].y_pixels) {
         preferred_mode_index = mode_index;
       }
    }
  }
  return preferred_mode_index;
}
};
