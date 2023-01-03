/*
 * Copyright (c) 2022, 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>
#include "sideband_test.h"

using namespace android;
using Transaction = SurfaceComposerClient::Transaction;
using android::sp;
using vendor::display::config::V1_21::IDisplayConfig;

using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::graphics::allocator::V2_0::IAllocator;
using android::hardware::graphics::common::V1_0::PixelFormat;
using android::hardware::graphics::common::V1_1::BufferUsage;
using android::hardware::graphics::mapper::V2_0::BufferDescriptor;
using android::hardware::graphics::mapper::V2_0::IMapper;
using android::hardware::graphics::mapper::V2_0::Error;
using android::hardware::graphics::mapper::V2_0::YCbCrLayout;
using std::vector;

volatile sig_atomic_t stop;

TunnellingHelper::TunnellingHelper() {
  Init();
}

void TunnellingHelper::Init() {
  allocator_ = IAllocator::getService();
  mapper_ = IMapper::castFrom(IMapper::getService());
  if (allocator_.get() == nullptr) {
    std::cout << "failed to get allocator service";
    return;
  }
  if (mapper_.get() == nullptr) {
    std::cout << "failed to get mapper service";
    return;
  }
}

vector<const native_handle_t *> TunnellingHelper::Allocate(
    const IMapper::BufferDescriptorInfo &desc_info, uint32_t count, uint32_t *out_stride) {
  vector<const native_handle_t *> handles;
  auto descriptor = CreateDescriptor(desc_info);
  allocator_->allocate(
      descriptor, count,
      [&](const auto &_error, const auto &_stride, const auto &_buffers) {
        if (Error::NONE != _error) {
           std::cout << "failed to allocate buffers";
           return;
        }
        if (count != _buffers.size()) {
           std::cout << "invalid buffer array";
           return;
        }
        for (uint32_t i = 0; i < count; i++) {
          handles.push_back(ImportBuffer(_buffers[i]));
        }
        if (out_stride) {
          *out_stride = _stride;
        }
      });

  return handles;
}

const native_handle_t *TunnellingHelper::ImportBuffer(const hidl_handle &raw_handle) {
  const native_handle_t *handle = nullptr;
  mapper_->importBuffer(raw_handle, [&](const auto &_error,
                                        const auto &_buffer) {
    if (Error::NONE != _error) {
       std::cout << "failed to import buffer %p" << raw_handle.getNativeHandle();
       return;
    }
    handle = static_cast<const native_handle_t *>(_buffer);
  });

  return handle;
}

void TunnellingHelper::FreeBuffer(const native_handle_t *buffer_handle) {
  auto buffer = const_cast<native_handle_t *>(buffer_handle);
  Error error =
      mapper_->freeBuffer(buffer);
  if (Error::NONE != error) {
     std::cout << "failed to free buffer " << buffer;
     return;
  }
}

BufferDescriptor TunnellingHelper::CreateDescriptor(
  const IMapper::BufferDescriptorInfo &descriptor_info) {
  auto descriptor = BufferDescriptor();
  mapper_->createDescriptor(descriptor_info, [&](const auto &_error,
                                                 const auto &_descriptor) {
    if (Error::NONE != _error) {
       std::cout << "failed to create descriptor";
       return;
    }
    descriptor = _descriptor;
  });
  return descriptor;
}

void memset24(void *p_dst, uint32_t value, int count) {
  uint8_t *ptr = (uint8_t *)p_dst,*end_ptr;
  uint8_t x, y, z;

  end_ptr = ptr + 3 * count;
  x = value & 0xFF;
  y = (value >> 8) & 0xFF;
  z = (value >> 16) & 0xFF;

  while (ptr < end_ptr) {
    *ptr++ = x;
    *ptr++ = y;
    *ptr++ = z;
  }
}

void sigint_handler(int signum) {
  signal(SIGINT, SIG_IGN);
  stop = 1;
  return;
}

void fillSurfaceRGBA8(const sp<SurfaceControl>& sc, uint8_t r, uint8_t g, uint8_t b,
                             bool unlock = true) {
    int usec_delay = (float) (1.0 / (float) kFps) * 1000000.0;
    ANativeWindow_Buffer outBuffer;
    sp<Surface> s = sc->getSurface();
    s->lock(&outBuffer, nullptr);
    uint8_t* img = reinterpret_cast<uint8_t*>(outBuffer.bits);
    for (int y = 0; y < outBuffer.height; y++) {
        for (int x = 0; x < outBuffer.width; x++) {
            uint8_t* pixel = img + (4 * (y * outBuffer.stride + x));
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
            pixel[3] = 255;
        }
    }
    if (unlock) {
        s->unlockAndPost();
    }
}

void render_buffer(sp<SurfaceControl> &layer, uint32_t duration) {
    SurfaceComposerClient::Transaction t={};
    uint32_t time_elapsed = 0;
    while(time_elapsed <= duration) {
      for(int i = 0; i < kNumBuffers; i++) {
        if (i < kNumBuffers/3) {
          fillSurfaceRGBA8(layer, 200, 200, 200);
          t.show(layer).apply(true);
        }
        else if (i < (2 * (kNumBuffers/3))) {
          fillSurfaceRGBA8(layer, 50, 50, 50);
          t.show(layer).apply(true);
        }
        else {
          fillSurfaceRGBA8(layer, 100, 100, 100);
          t.show(layer).apply(true);
        }
        t.setPosition(layer,500,500).apply();
      }
      time_elapsed++;
    }
}

int send_buffers(uint32_t inWidth, uint32_t inHeight, int format,
                 uint32_t duration, uint32_t num_buffers) {
  int err = 0;
  IMapper::BufferDescriptorInfo info = {
    .width = inWidth > 0 ? inWidth : 500,
    .height = inHeight > 0 ? inHeight : 500,
    .layerCount = 1,
    .format = static_cast<android::hardware::graphics::common::V1_0::PixelFormat>(format),
    .usage = static_cast<uint64_t>(BufferUsage::CPU_WRITE_MASK),
  };
  std::unique_ptr<TunnellingHelper> gralloc_;
  gralloc_ = std::make_unique<TunnellingHelper>();
  sp<IDisplayConfig>  mDisplayConfig = IDisplayConfig::getService();
  if (mDisplayConfig == NULL) {
    std::cout << "Could not load service IDisplayConfig" <<std::endl;
    return 0;
  }
  auto handles = gralloc_->Allocate(info, kNumBuffers);
  int fd[kNumBuffers];
  for(int i = 0; i < kNumBuffers; i++) {
    auto hnd = (private_handle_t *) handles[i];
    gralloc_->handle_release_fence_map_[hnd] = -1;
    hnd->Dump(hnd);
    fd[i] = hnd->fd;
    int width = hnd->width;
    int height = hnd->height;
    unsigned int size = hnd->size;
    printf("Buffer fd = %d, width x height: %d x %d, size = %d\n",
            hnd->fd, width, height, size);
    void* cpudaddr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd[i], 0);
    memset(cpudaddr, 0x00, size);

    char *buffer = ((char *) cpudaddr);
    uint32_t long_color_val;
    if (i < kNumBuffers/3)
      long_color_val = 0xFFFF00;
    else if (i < (2 * (kNumBuffers/3)))
      long_color_val = 0xFFFFFF;
    else
      long_color_val = 0x000000;
    for (int row = 0; row < height; row++) {
      memset24(buffer + (width * 3) * row, long_color_val, width);
    }
  }

  signal(SIGINT, sigint_handler);

  int frames_elapsed = 0;
  uint32_t time_elapsed = 0;
  int usec_delay = (float) (1.0 / (float) kFps) * 1000000.0;

  while(!stop && time_elapsed <= duration) {
    for(int i = 0; i < kNumBuffers; i++) {
      auto hnd = (private_handle_t *) handles[i];
      int release_fence_fd = gralloc_->handle_release_fence_map_[hnd];
      if (release_fence_fd >= 0) {
        err = sync_wait(release_fence_fd, 16);
        if (err < 0) {
          ALOGE("Fence did not signal, fd : %d",release_fence_fd);
        } else {
          ALOGE("Fence signalled, fd : %d",release_fence_fd);
        }
        close(release_fence_fd);
      }

      // Client should pass real acquire fence handle below instead of NULL
      // NULL acquire fence handle will be considered as -1 acquire fence fd
      auto error = mDisplayConfig->queueTunnelledBuffer(hnd, NULL /*acquire_fence_handle*/);
      if (!error.isOk())
        continue;

      if (error == EINVAL) {
        stop = true;
        break;
      }

      usleep(usec_delay);

      // Logic to make the loop time bound
      frames_elapsed++;
      frames_elapsed %= kFps;
      if (frames_elapsed == 0 && duration > 0) time_elapsed++;

      int32_t release_fence = -1;
      auto error_dequeue = mDisplayConfig->dequeueTunnelledBuffer(hnd, [&](const auto& tmpError,
         const auto& tmpHandle) {
         err = tmpError;
         if (tmpHandle != NULL) {
           const native_handle_t* nativeFenceHandle = tmpHandle.getNativeHandle();
           if (nativeFenceHandle != nullptr) {
             release_fence = dup(nativeFenceHandle->data[0]);
             close(nativeFenceHandle->data[0]);
             native_handle_close(nativeFenceHandle);
           }
         }
      });
      if (!error_dequeue.isOk())
         continue;

      if (err == EINVAL) {
        stop = true;
        break;
      }

      if (release_fence != -1) {
        gralloc_->handle_release_fence_map_[hnd] = release_fence;
        ALOGI("Duped fd = %d ...for hnd i = %d", release_fence,i);
      }
    }
  }

  for(int i = 0; i < kNumBuffers; i++) {
    gralloc_->FreeBuffer(handles[i]);
  }

  mDisplayConfig.clear();
  mDisplayConfig = NULL;
  return 0;
}
