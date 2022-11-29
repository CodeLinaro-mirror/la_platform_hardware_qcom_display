/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <map>
#include <sys/mman.h>
#include <gr_utils.h>
#include <iostream>
#include <sync/sync.h>

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>

#include <vendor/display/config/1.21/IDisplayConfig.h>

using namespace android;
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

static const int kNumMsec = 16660;
static const int kNumBuffers = 30;
volatile sig_atomic_t stop;
// Wraps shared gralloc functions
class TunnellingHelper {
 public:
  TunnellingHelper();
  ~TunnellingHelper() = default;

  vector<const native_handle_t *> Allocate(const IMapper::BufferDescriptorInfo &desc_info,
                                           uint32_t count, uint32_t *out_stride = nullptr);

  const native_handle_t *ImportBuffer(const hidl_handle &raw_handle);
  void FreeBuffer(const native_handle_t *buffer_handle);

  BufferDescriptor CreateDescriptor(const IMapper::BufferDescriptorInfo &descriptorInfo);

  std::map<private_handle_t *,int > handle_release_fence_map_;

private:
  void Init();
  sp<IAllocator> allocator_;
  sp<IMapper> mapper_;
};

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

void memset24(void *p_dst, uint32_t value, int count)
{
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

void sigint_handler(int signum)
{
  signal(SIGINT, SIG_IGN);
  stop = 1;
  return;
}

int main(int argc, char **argv) {
  IMapper::BufferDescriptorInfo info = {
    .width = 500,
    .height = 500,
    .layerCount = 1,
    .format = static_cast<PixelFormat>(HAL_PIXEL_FORMAT_RGB_888),
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

  IDisplayConfig::LayerInfo layer;
  layer.z_order = 3000;
  layer.dataspace = HAL_DATASPACE_SRGB;
  layer.layer_transform = IDisplayConfig::transform_t::TRANSFORM_FLIP_H;
  layer.dst_rect.left = 0;
  layer.dst_rect.top = 0;
  layer.dst_rect.right = 500;
  layer.dst_rect.bottom = 500;

  layer.src_rect.left = 0;
  layer.src_rect.top = 0;
  layer.src_rect.right = 500;
  layer.src_rect.bottom = 500;

  layer.buffer_fd = fd[0];
  layer.width = 512;
  layer.height = 500;
  layer.unaligned_width = 500;
  layer.unaligned_height = 500;
  layer.format = HAL_PIXEL_FORMAT_RGB_888;
  layer.plane_alpha = 1;
  layer.blending = IDisplayConfig::blend_mode::BLEND_MODE_PREMULTIPLIED;

  int err = mDisplayConfig->tunnellingInit();
  if (err != 0) {
    return 0;
  }

  err = mDisplayConfig->createTunnelledLayer(layer);
  if (err != 0) {
    return 0;
  }

  signal(SIGINT, sigint_handler);

  while(!stop) {
    for(int i = 0; i < kNumBuffers; i++) {
      auto hnd = (private_handle_t *)handles[i];
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
      ALOGI("Before queue buffer");
      // Client should pass real acquire fence handle below instead of NULL
      // NULL acquire fence handle will be considered as -1 acquire fence fd
      auto error = mDisplayConfig->queueTunnelledBuffer(hnd, NULL /*acquire_fence_handle*/);
      if (!error.isOk())
        continue;

      if (error == EINVAL) {
        stop = true;
        break;
      }

      usleep(kNumMsec);
      int32_t release_fence = -1;
      ALOGI("Before dequeue buffer\n");
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

  err = mDisplayConfig->destroyTunnelledLayer();
  if (err != 0) {
    ALOGE("Destroy layer failed");
  }

  err = mDisplayConfig->tunnellingDeinit();
  if (err != 0) {
    ALOGE("tunnellingDeinit failed");
  }

  for(int i = 0; i < kNumBuffers; i++) {
    gralloc_->FreeBuffer(handles[i]);
  }

  mDisplayConfig.clear();
  mDisplayConfig = NULL;
}
