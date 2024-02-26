/*
 * Copyright (c) 2016-2017, 2019 The Linux Foundation. All rights reserved.
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
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <cutils/native_handle.h>
#include <ui/GraphicBuffer.h>
#include <map>
#include "EGLImageWrapper.h"
#include "glengine.h"

//-----------------------------------------------------------------------------
EGLImageKHR create_eglImage(android::sp<android::GraphicBuffer> graphicBuffer)
//-----------------------------------------------------------------------------
{
  bool isProtected = (graphicBuffer->getUsage() & GRALLOC_USAGE_PROTECTED);
  EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                    isProtected ? EGL_PROTECTED_CONTENT_EXT : EGL_NONE,
                    isProtected ? EGL_TRUE : EGL_NONE, EGL_NONE};

  EGLImageKHR eglImage = eglCreateImageKHR(
      eglGetCurrentDisplay(), (EGLContext)EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
      (EGLClientBuffer)(graphicBuffer->getNativeBuffer()), attrs);

  return eglImage;
}

//-----------------------------------------------------------------------------
EGLImageBuffer::EGLImageBuffer(android::sp<android::GraphicBuffer> graphicBuffer)
//-----------------------------------------------------------------------------
{
  // this->graphicBuffer = graphicBuffer;
  this->eglImageID = create_eglImage(graphicBuffer);
  this->width = graphicBuffer->getWidth();
  this->height = graphicBuffer->getHeight();

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
      ALOGI("%s Framebuffer Invalid***************", __FUNCTION__);
    }
  }

  GL(glBindFramebuffer(GL_FRAMEBUFFER, framebufferID));
}
