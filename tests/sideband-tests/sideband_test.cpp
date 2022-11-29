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

#include <gtest/gtest.h>

#include <android/native_window.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include <ui/DisplayInfo.h>

#include "sideband_test.h"

namespace android {

using Transaction = SurfaceComposerClient::Transaction;
using ui::ColorMode;

namespace {
const String8 SURFACE_NAME("Sideband Test Surface");
} // namespace

/**
 * This class tests the SetLayerSidebandStream method in SurfaceFlinger.
 */
class SidebandStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_NO_FATAL_FAILURE(initClient());
    }

    void TearDown() override {
        mComposerClient->dispose();
        mSurfaceControl.clear();
        mComposerClient.clear();
    }

    sp<IBinder> mDisplay;
    sp<SurfaceComposerClient> mComposerClient = nullptr;
    sp<SurfaceControl> mSurfaceControl = nullptr;
    SurfaceComposerClient::Transaction transaction = {};

    void initClient() {
        mComposerClient = new SurfaceComposerClient;
        ASSERT_EQ(NO_ERROR, mComposerClient->initCheck());
    }

    void setupSidebandSurface(int width=500, int height=500,
                              int z=INT_MAX-3, bool fullscreen=false) {
        mDisplay = SurfaceComposerClient::getInternalDisplayToken();
        ASSERT_FALSE(mDisplay == nullptr);

        DisplayInfo info;
        ASSERT_EQ(NO_ERROR, SurfaceComposerClient::getDisplayInfo(mDisplay, &info));
        const ssize_t surfaceWidth = fullscreen ? info.w : width;
        const ssize_t surfaceHeight = fullscreen ? info.h : height;

        mSurfaceControl =
                mComposerClient->createSurface(SURFACE_NAME, surfaceWidth, surfaceHeight,
                                               PIXEL_FORMAT_RGB_888, 0);
        ASSERT_TRUE(mSurfaceControl != nullptr);
        ASSERT_TRUE(mSurfaceControl->isValid());

        transaction = {};
        ASSERT_EQ(NO_ERROR,
                  transaction.setLayer(mSurfaceControl, z).apply());

        transaction = {};
        sp<Surface> surface = mSurfaceControl->getSurface();
        ANativeWindow_Buffer surfaceBuffer;
        surface->lock(&surfaceBuffer, NULL);
        size_t buf_size = surfaceBuffer.stride * surfaceBuffer.height *
                          bytesPerPixel(surfaceBuffer.format);
        printf("width: %d, height: %d, stride: %d, format: %d\n", surfaceBuffer.width,
                surfaceBuffer.height, surfaceBuffer.stride, surfaceBuffer.format);
        ASSERT_EQ(NO_ERROR, transaction.apply());

        transaction = {};
        memset(surfaceBuffer.bits, 0x00, buf_size);
        ASSERT_EQ(NO_ERROR, transaction.apply());

        transaction = {};
        sp<ANativeWindow> window(surface);
        ANativeWindowBuffer *anw;
        native_window_api_connect(window.get(), NATIVE_WINDOW_API_CPU);
        native_window_dequeue_buffer_and_wait(window.get(), &anw);
        sp<NativeHandle> stream = android::NativeHandle::create(
                                const_cast<native_handle*>(anw->handle), false);
        surface->setSidebandStream(stream);
        ASSERT_EQ(NO_ERROR, transaction.apply());

        surface->unlockAndPost();
    }

    void queueBuffers() {
        send_buffers();
    }
};

TEST_F(SidebandStreamTest, QueueSidebandStreamBufferTest) {
    setupSidebandSurface();
    printf("Surface Sideband Stream setup successfully, now queueing buffers...\n");
    queueBuffers();
}

} // namespace android

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}