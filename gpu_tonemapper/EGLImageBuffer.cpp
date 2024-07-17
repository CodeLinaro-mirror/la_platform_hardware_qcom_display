/*
 * Copyright (c) 2016-2017, 2019 The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright 2015 The Android Open Source Project
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "EGLImageBuffer.h"
#include "EGLImageWrapper.h"
#include <EGL/eglwaylandext.h>

using namespace drm_utils;

//-----------------------------------------------------------------------------
EGLImageKHR EGLImageBuffer::create_eglImage(struct gbm_buf_info *gbo_info, void *userdata)
//-----------------------------------------------------------------------------
{
  unsigned int secure_status = 0;
  EGLImageKHR eglImage;
  PFNEGLCREATEIMAGEKHRPROC create_image;
  create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>
                                  (eglGetProcAddress("eglCreateImageKHR"));

  EGLint attribs[20];
  struct gbm_bo *bo = NULL;
  int i=0;

  bo = gbm_bo_import(gbm_, GBM_BO_IMPORT_GBM_BUF_TYPE, gbo_info,
                        GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);

  gbm_perform(GBM_PERFORM_GET_SECURE_BUFFER_STATUS, bo, &secure_status);
  //We need to pass wl_resource to create egl image to support TP10_UBWC and NV12 formats in
  // Forward tone mapper
  if(gbo_info->format == GBM_FORMAT_YCbCr_420_TP10_UBWC || gbo_info->format == GBM_FORMAT_NV12) {
    attribs[i++] = EGL_WAYLAND_PLANE_WL;
    attribs[i++] = 0;
    attribs[i++] = EGL_PROTECTED_CONTENT_EXT;
    attribs[i++] = secure_status;
    attribs[i++] = EGL_NONE;

    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(userdata);
    eglImage = create_image(eglGetCurrentDisplay(), (EGLContext)EGL_NO_CONTEXT,
                                      EGL_WAYLAND_BUFFER_WL, buffer, attribs);
  } else {
    attribs[i++] = EGL_WIDTH;
    attribs[i++] = gbm_bo_get_width(bo);
    attribs[i++] = EGL_HEIGHT;
    attribs[i++] = gbm_bo_get_height(bo);
    attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[i++] = gbm_bo_get_format(bo);
    attribs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attribs[i++] = gbm_bo_get_fd(bo);
    attribs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attribs[i++] = 0;
    attribs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attribs[i++] = gbm_bo_get_stride(bo);
    attribs[i++] = EGL_PROTECTED_CONTENT_EXT;
    attribs[i++] = secure_status;
    attribs[i++] = EGL_NONE;

    eglImage = create_image(eglGetCurrentDisplay(), (EGLContext)EGL_NO_CONTEXT,
                                             EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
  }
   // we no longer need the bo
  if (bo) {
    gbm_bo_destroy(bo);
  }

  return eglImage;
}

//-----------------------------------------------------------------------------
EGLImageBuffer::EGLImageBuffer(struct gbm_buf_info *gbuf_info, void *userdata, void *userdata2)
//-----------------------------------------------------------------------------
{

  struct gbm_buf_info *gbo_info = gbuf_info;

  DRMMaster *master = nullptr;
  int ret = DRMMaster::GetInstance(&master);

  if (ret < 0) {
      fprintf(stderr, "Failed to acquire DRMMaster instance\n");
  }

  master->GetHandle(&fd);

  gbm_ = (gbm_device*) userdata2;

  this->eglImageID = create_eglImage(gbo_info, userdata);
  this->width = gbo_info->width;
  this->height = gbo_info->height;

  textureID = 0;
  renderbufferID = 0;
  framebufferID = 0;
}

//-----------------------------------------------------------------------------
EGLImageBuffer::~EGLImageBuffer()
//-----------------------------------------------------------------------------
{
  if (textureID != 0) {
    GL(glDeleteTextures(1, &textureID));
    textureID = 0;
  }

  if (renderbufferID != 0) {
    GL(glDeleteRenderbuffers(1, &renderbufferID));
    renderbufferID = 0;
  }

  if (framebufferID != 0) {
    GL(glDeleteFramebuffers(1, &framebufferID));
    framebufferID = 0;
  }

  // Delete the eglImage
  if (eglImageID != 0)
  {
      eglDestroyImageKHR(eglGetCurrentDisplay(), eglImageID);
      eglImageID = 0;
  }

  /* static variable initialized is for 2 purpose: */
  /* 1: to help initialize by getting master fd and opening gbm device first time */
  /* 2: On every buffer creation instance, a reference count is added to it to    */
  /*    keep track of how many times this object has been instantiation. It is    */
  /*    decremented in destructor. But if the object to be destroyed is with      */
  /*    reference count = 1, then fd is set to invalid number and gbm device is   */
  /*    destroyed */

  fd = -1;
}

//-----------------------------------------------------------------------------
int EGLImageBuffer::getWidth()
//-----------------------------------------------------------------------------
{
  return width;
}

//-----------------------------------------------------------------------------
int EGLImageBuffer::getHeight()
//-----------------------------------------------------------------------------
{
  return height;
}

//-----------------------------------------------------------------------------
unsigned int EGLImageBuffer::getTexture(int target)
//-----------------------------------------------------------------------------
{
  if (textureID == 0) {
    bindAsTexture(target);
  }

  return textureID;
}

//-----------------------------------------------------------------------------
unsigned int EGLImageBuffer::getFramebuffer()
//-----------------------------------------------------------------------------
{
  if (framebufferID == 0) {
    bindAsFramebuffer();
  }

  return framebufferID;
}

//-----------------------------------------------------------------------------
void EGLImageBuffer::bindAsTexture(int target)
//-----------------------------------------------------------------------------
{
  if (textureID == 0) {
    GL(glGenTextures(1, &textureID));
    GL(glBindTexture(target, textureID));
    GL(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GL(glEGLImageTargetTexture2DOES(target, eglImageID));
  }

  GL(glBindTexture(target, textureID));
}

//-----------------------------------------------------------------------------
void EGLImageBuffer::bindAsFramebuffer()
//-----------------------------------------------------------------------------
{
  if (renderbufferID == 0) {
    GL(glGenFramebuffers(1, &framebufferID));
    GL(glGenRenderbuffers(1, &renderbufferID));

    GL(glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID));
    GL(glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, eglImageID));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, framebufferID));
    GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                 renderbufferID));
    GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (result != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "%s Framebuffer Invalid***************", __FUNCTION__);
    }
  }

  GL(glBindFramebuffer(GL_FRAMEBUFFER, framebufferID));
}
