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
static const int kFps = 60;
static const int kNumBuffers = 30;

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

void memset24(void *p_dst, uint32_t value, int count);
void sigint_handler(int signum);
void render_buffer(sp<SurfaceControl> &layer,uint32_t duration);
int send_buffers(uint32_t width, uint32_t height, int format,
                 uint32_t duration, uint32_t num_buffers);
