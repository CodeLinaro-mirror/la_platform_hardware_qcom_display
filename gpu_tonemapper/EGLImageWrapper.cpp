/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * ​​​​​Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "EGLImageWrapper.h"

using std::map;

//-----------------------------------------------------------------------------
EGLImageWrapper::EGLImageWrapper()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
EGLImageWrapper::~EGLImageWrapper()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
EGLImageBuffer *EGLImageWrapper::wrap(void *buf_info, void *userdata, void *userdata2)
//-----------------------------------------------------------------------------
{
  struct gbm_buf_info *src = reinterpret_cast<struct gbm_buf_info *>(buf_info);
  EGLImageBuffer *result = 0;

  map<int, EGLImageBuffer *>::iterator it = eglImageBufferMap.find(src->fd);

  if (it == eglImageBufferMap.end()) {
    result = new EGLImageBuffer(src, userdata, userdata2);
    eglImageBufferMap[src->fd] = result;
  } else {
    result = it->second;
  }

  return result;
}

//-----------------------------------------------------------------------------
void EGLImageWrapper::destroy()
//-----------------------------------------------------------------------------
{
  map<int, EGLImageBuffer *>::iterator it = eglImageBufferMap.begin();
  for (; it != eglImageBufferMap.end(); it++) {
    delete it->second;
  }
  eglImageBufferMap.clear();
}