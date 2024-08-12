/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "EGLImageWrapper.h"
#include "Tonemapper.h"
#include "engine.h"
#include "forward_tonemap.inl"
#include "fullscreen_vertex_shader.inl"
#include "rgba_inverse_tonemap.inl"

//-----------------------------------------------------------------------------
Tonemapper::Tonemapper()
//-----------------------------------------------------------------------------
{
  tonemapTexture = 0;
  lutXformTexture = 0;
  programID = 0;
  eglImageWrapper = new EGLImageWrapper();

  lutXformScaleOffset[0] = 1.0f;
  lutXformScaleOffset[1] = 0.0f;

  tonemapScaleOffset[0] = 1.0f;
  tonemapScaleOffset[1] = 0.0f;
}

//-----------------------------------------------------------------------------
Tonemapper::~Tonemapper()
//-----------------------------------------------------------------------------
{
  engine_bind(engineContext);
  engine_deleteInputBuffer(tonemapTexture);
  engine_deleteInputBuffer(lutXformTexture);
  engine_deleteProgram(programID);

  // clear EGLImage mappings
  if (eglImageWrapper != 0) {
    delete eglImageWrapper;
    eglImageWrapper = 0;
  }

  engine_shutdown(engineContext);
}

//-----------------------------------------------------------------------------
Tonemapper *Tonemapper::build(int type, void *colorMap, int colorMapSize, void *lutXform,
                              int lutXformSize, bool isSecure)
//-----------------------------------------------------------------------------
{
  if (colorMapSize <= 0) {
      fprintf(stderr, "Invalid Color Map size = %d", colorMapSize);
      return NULL;
  }

  // build new tonemapper
  Tonemapper *tonemapper = new Tonemapper();

  tonemapper->engineContext = engine_initialize(isSecure);

  engine_bind(tonemapper->engineContext);

  // load the 3d lut
  tonemapper->tonemapTexture = engine_load3DTexture(colorMap, colorMapSize, 0);
  tonemapper->tonemapScaleOffset[0] = ((float)(colorMapSize-1))/((float)(colorMapSize));
  tonemapper->tonemapScaleOffset[1] = 1.0f/(2.0f*colorMapSize);

  // load the non-uniform xform
  tonemapper->lutXformTexture = engine_load1DTexture(lutXform, lutXformSize, 0);
  bool bUseXform = (tonemapper->lutXformTexture != 0) && (lutXformSize != 0);
  if( bUseXform )
  {
      tonemapper->lutXformScaleOffset[0] = ((float)(lutXformSize-1))/((float)(lutXformSize));
      tonemapper->lutXformScaleOffset[1] = 1.0f/(2.0f*lutXformSize);
  }

  // create the program
  const char *fragmentShaders[3];
  int fragmentShaderCount = 0;
  const char *version = "#version 300 es\n";
  const char *define = "#define USE_NONUNIFORM_SAMPLING\n";

  fragmentShaders[fragmentShaderCount++] = version;

  // non-uniform sampling
  if (bUseXform) {
    fragmentShaders[fragmentShaderCount++] = define;
  }

  if (type == TONEMAP_INVERSE) {  // inverse tonemapping
    fragmentShaders[fragmentShaderCount++] = rgba_inverse_tonemap_shader;
  } else {  // forward tonemapping
    fragmentShaders[fragmentShaderCount++] = forward_tonemap_shader;
  }

  tonemapper->programID =
      engine_loadProgram(1, &fullscreen_vertex_shader, fragmentShaderCount, fragmentShaders);

  return tonemapper;
}

//-----------------------------------------------------------------------------
int Tonemapper::blit(void *dst, void *src, int srcFenceFd, void *userdata, void *userdata2)
//-----------------------------------------------------------------------------
{
  // make current
  engine_bind(engineContext);

  // create eglimages if required
  EGLImageBuffer *dst_buffer = eglImageWrapper->wrap(dst, userdata, userdata2);
  EGLImageBuffer *src_buffer = eglImageWrapper->wrap(src, userdata, userdata2);

  // bind the program
  engine_setProgram(programID);

  engine_setData2f(3, tonemapScaleOffset);
  bool bUseXform = (lutXformTexture != 0);
  if( bUseXform )
  {
    engine_setData2f(4, lutXformScaleOffset);
  }

  // set destination
  if (dst_buffer) {
    engine_setDestination(dst_buffer->getFramebuffer(), 0, 0, dst_buffer->getWidth(),
                          dst_buffer->getHeight());
  }
  // set source
  if (src_buffer) {
    engine_setExternalInputBuffer(0, src_buffer->getTexture(0x8D65 /* target texture */));
  }
  // set 3d lut
  engine_set3DInputBuffer(1, tonemapTexture);
  // set non-uniform xform
  engine_set2DInputBuffer(2, lutXformTexture);

  // TODO: Check if needed
  // set dimensions
  float dimensions[2] = { 0.0f, 0.0f };
  dimensions[0] = dst_buffer->getWidth();
  dimensions[1] = dst_buffer->getHeight();
  engine_setData2f(5, dimensions);

  // perform
  int fenceFD = engine_blit(srcFenceFd);

  return fenceFD;
}
