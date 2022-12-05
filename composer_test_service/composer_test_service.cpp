/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <thread>
#include <chrono>
#include <errno.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <hidl/LegacySupport.h>

#include "composer_test_service.h"
#include "comp_test_bnd_service.h"
#include "sdm_comp_debugger.h"

#define __CLASS__ "ComposerTestService"

using namespace android;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

namespace sdm {

int ComposerTestService::Init() {
  /* Enable verbose logging */
  SDMCompDebugHandler::DebugAll(1, 1);

  /* Create Core Interface */
  std::bitset<8> core_ids(2);
  DisplayError error = CoreInterface::CreateCore(&buffer_allocator_, nullptr,
                                                 nullptr /*socket_handler*/,
                                                 nullptr /*ipc_intf*/, &core_intf_,
                                                 core_ids);
  if (error != kErrorNone) {
    DLOGE("Failed to create Core Interface");
    return -EINVAL;
  }
  DLOGI("Created Core Interface");

  /* Create Builtin Display */
  HWDisplaysInfo hw_displays_info = {};
  error = core_intf_->GetDisplaysStatus(&hw_displays_info);
  if (error != kErrorNone) {
    DLOGE("Failed to get connected display list. Error = %d", error);
    return -EINVAL;
  }

  for (auto &iter : hw_displays_info) {
    auto &info = iter.second;

    if (info.display_type != kBuiltIn) {
      continue;
    }

    // Enable check once driver reports primary display
    /*
    if (!info.is_primary) {
      continue;
    }*/

    if (!info.is_connected) {
      continue;
    }

    DLOGI("Create builtin display, id = %d, type = %d", info.display_id,
          kSDMCompDisplayTypePrimary);
    SDMCompDisplayBuiltIn *display_builtin = new SDMCompDisplayBuiltIn(core_intf_,
                                                                       kSDMCompDisplayTypePrimary,
                                                                       info.display_id);
    if (!display_builtin) {
      break;
    }
    int status = display_builtin->Init();
    if (status) {
      DLOGE("Failed to initialize SDMCompDisplayBuiltIn, error = %d", status);
      delete display_builtin;
      return status;
    }
    display_builtin_ = display_builtin;
    break;
  }

  if (!display_builtin_) {
    DLOGE("Display creation failed!");
    return -EINVAL;
  }

  DLOGI("Display Creation Successful");
  return 0;
}

int ComposerTestService::CommandHandler(uint32_t command, const android::Parcel *input_parcel,
                     android::Parcel *output_parcel) {
  int status = 0;
  switch (command) {
    case ICompTestBndService::COMP_START_TEST:
      status = RunTest();
      break;
    case ICompTestBndService::SET_QSYNC_MODE:
      if (!input_parcel) {
        DLOGE("QService command = %d: input_parcel needed.", command);
        break;
      }
      status = SetQSyncMode(input_parcel);
      break;
    default:
      status = -EINVAL;
      DLOGE("QService command = %d is not supported.", command);
      break;
  }
  return status;
}

int ComposerTestService::RunTest() {
  int ret = Init();
  if (ret != 0) {
    DLOGE("Error initializing ComposerTestService, err %d", ret);
    Deinit();
    return ret;
  }
  DLOGI("DisplayContent start...");
  DisplayContent();
  DLOGI("DisplayContent end...");

  ret = Deinit();
  if (ret != 0) {
    DLOGE("Error deinitializing ComposerTestService, err %d", ret);
    return ret;
  }
  return 0;
}

int ComposerTestService::SetQSyncMode(const android::Parcel *input_parcel) {
  auto mode = input_parcel->readInt32();

  QSyncMode qsync_mode = kQSyncModeNone;
  switch (mode) {
    case ICompTestBndService::QSYNC_MODE_NONE:
      qsync_mode = kQSyncModeNone;
      break;
    case ICompTestBndService::QSYNC_MODE_CONTINUOUS:
      qsync_mode = kQSyncModeContinuous;
      break;
    case ICompTestBndService::QSYNC_MODE_ONESHOT:
      qsync_mode = kQsyncModeOneShot;
      break;
    default:
      DLOGE("Qsync mode not supported %d", mode);
      return -EINVAL;
  }

  qsync_mode_ = qsync_mode;
  return 0;
}

int ComposerTestService::Deinit() {
  /* Destroy Builtin Display */
  DLOGI("Destroying builtin display");
  if (display_builtin_) {
    int status = display_builtin_->Deinit();
    delete display_builtin_;
    display_builtin_ = nullptr;
  }

  /* Destory Core */
  if (core_intf_) {
    DisplayError error = CoreInterface::DestroyCore();
    if (error != kErrorNone) {
      DLOGE("Display core de-initialization failed. Error = %d", error);
      return -EINVAL;
    }
    core_intf_ = nullptr;
  }
  return 0;
}

int ComposerTestService::DisplayContent() {
  /* Set panel brightness so that content is visible */
  display_builtin_->SetPanelBrightness(1);

  struct BufferInfo buffer_info[5];
  string file_path[5];
  const native_handle_t *handle[5] = {};

  DLOGI("Expected File-1 /sdcard/DriverTestUbwc4_1080x1920_ABGR_8888_UBWC.rgb");
  file_path[0] = "/sdcard/DriverTestUbwc4_1080x1920_ABGR_8888_UBWC.rgb";
  buffer_info[0].buffer_config.width = 1080;
  buffer_info[0].buffer_config.height = 1920;
  buffer_info[0].buffer_config.format = kFormatRGBA8888Ubwc;

  DLOGI("Expected File-2 /sdcard/Image_1280x2408_RGB_888_1.RAW");
  file_path[1] = "/sdcard/Image_1280x2408_RGB_888_1.RAW";
  buffer_info[1].buffer_config.width = 1280;
  buffer_info[1].buffer_config.height = 2408;
  buffer_info[1].buffer_config.format = kFormatRGB888;

  DLOGI("Expected File-3 /sdcard/Image_1280x2408_RGB_888_2.RAW");
  file_path[2] = "/sdcard/Image_1280x2408_RGB_888_2.RAW";
  buffer_info[2].buffer_config.width = 1280;
  buffer_info[2].buffer_config.height = 2408;
  buffer_info[2].buffer_config.format = kFormatRGB888;

  DLOGI("Expected File-4 /sdcard/Image_1280x2408_RGB_888_3.RAW");
  file_path[3] = "/sdcard/Image_1280x2408_RGB_888_3.RAW";
  buffer_info[3].buffer_config.width = 1280;
  buffer_info[3].buffer_config.height = 2408;
  buffer_info[3].buffer_config.format = kFormatRGB888;

  DLOGI("Expected File-5 /sdcard/Image_1280x2408_RGB_888_4.RAW");
  file_path[4] = "/sdcard/Image_1280x2408_RGB_888_4.RAW";
  buffer_info[4].buffer_config.width = 1280;
  buffer_info[4].buffer_config.height = 2408;
  buffer_info[4].buffer_config.format = kFormatRGB888;

  for (int i = 0; i < 5; i++) {
    char *buffer = nullptr;
    FILE *fp = fopen(file_path[i].c_str(), "rb");
    if (fp == NULL) {
      DLOGE("Unable to open file %s", file_path[i].c_str());
      goto cleanup;
    }

    /* Allocate and Map Buffer */
    int ret = buffer_allocator_.AllocateBuffer(&buffer_info[i]);
    if (ret < 0) {
      DLOGE("Buffer Allocation failed ret %d", ret);
      fclose(fp);
      goto cleanup;
    }

    handle[i] = reinterpret_cast<const native_handle_t *>(buffer_info[i].private_data);
    ret = buffer_allocator_.MapBuffer(handle[i], nullptr, (void **)&buffer);
    if (ret < 0) {
      DLOGE("Buffer Mapping failed ret %d", ret);
      int ret2 = buffer_allocator_.FreeBuffer(&buffer_info[i]);
      if (ret2 < 0) {
        DLOGE("Failed to free buffer ret %d", ret2);
      }
      fclose(fp);
      handle[i] = nullptr;
      goto cleanup;
    }

    /*Copy Image to be displayed into Buffer*/
    fread(buffer, 1, buffer_info[i].alloc_buffer_info.size, fp);
    fclose(fp);

    BufferConfig &buffer_config = buffer_info[i].buffer_config;
    AllocatedBufferInfo &alloc_buffer_info = buffer_info[i].alloc_buffer_info;

    buffer_handle_[i].fd = alloc_buffer_info.fd;
    buffer_handle_[i].width = buffer_config.width;
    buffer_handle_[i].height = buffer_config.height;
    buffer_handle_[i].format = alloc_buffer_info.format;
    buffer_handle_[i].aligned_width = alloc_buffer_info.aligned_width;
    buffer_handle_[i].aligned_height = alloc_buffer_info.aligned_height;
    buffer_handle_[i].size = alloc_buffer_info.size;
    buffer_handle_[i].stride_in_bytes = alloc_buffer_info.stride;
    buffer_handle_[i].buffer_id = alloc_buffer_info.id;
  }

  /* Display Buffer */
  for (int i = 0; i < 1000 /* no of frames */; i++) {
    /* Set Qsync Mode */
    display_builtin_->SetQSyncMode(qsync_mode_);

    DLOGI("Queuing frame no: %d", i);
    int j = (i / 30) % 5;
    shared_ptr<Fence> retire_fence = nullptr;
    display_builtin_->ShowBuffer(&buffer_handle_[j], &retire_fence);
    if (retire_fence) {
      if (Fence::Wait(retire_fence) != 0) {
        DLOGW("sync_wait failed on retire_fence");
        goto cleanup;
      }
    }
    DLOGI("Start displaying frame no: %d", i);
  }

cleanup:
  for (int i = 0; i < 5; i++) {
    if (!handle[i]) {
      break;
    }
    int release_fence = -1;
    int ret = buffer_allocator_.UnmapBuffer(handle[i], &release_fence);
    if (ret < 0) {
      DLOGE("Buffer Unmapping failed err %d", ret);
    }
    ret = buffer_allocator_.FreeBuffer(&buffer_info[i]);
    if (ret < 0) {
      DLOGE("Failed to free buffer err %d", ret);
    }
  }
  return 0;
}

}  // namespace sdm

int main(int, char **) {
  ProcessState::initWithDriver("/dev/vndbinder");
  sp<ProcessState> ps(ProcessState::self());
  ps->setThreadPoolMaxThreadCount(4);
  ps->startThreadPool();

  sdm::ComposerTestService composer_test_service;
  configureRpcThreadpool(4, true /*callerWillJoin*/);
  CompTestBndService::Init(&composer_test_service);

  joinRpcThreadpool();

  return 0;
}
