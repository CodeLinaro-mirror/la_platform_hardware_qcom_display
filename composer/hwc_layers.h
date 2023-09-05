/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __HWC_LAYERS_H__
#define __HWC_LAYERS_H__

/* This class translates HWC2 Layer functions to the SDM LayerStack
 */

#include <gralloc_priv.h>
#include <qdMetaData.h>
#include <core/layer_stack.h>
#include <core/layer_buffer.h>
#include <utils/utils.h>
#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11
#include <deque>
#include <map>
#include <set>
#include "core/buffer_allocator.h"
#include "hwc_buffer_allocator.h"
#include "hwc_common.h"

using aidl::android::hardware::graphics::composer3::PerFrameMetadataKey;

namespace sdm {

DisplayError SetCSC(const private_handle_t *pvt_handle, ColorMetaData *color_metadata);
bool GetColorPrimaryAndMatrixCoef(const int32_t &dataspace,
                                  ColorPrimaries *color_primary,
                                  MatrixCoEfficients *matrix_coefficients);
bool GetTransfer(const int32_t &dataspace, GammaTransfer *gamma_transfer);
bool GetRange(const int32_t &dataspace, ColorRange *color_range);
bool GetSDMColorSpace(const int32_t &dataspace, ColorMetaData *color_metadata);
bool IsBT2020(const ColorPrimaries &color_primary);
int32_t TranslateFromLegacyDataspace(const int32_t &legacy_ds);

enum GeometryChanges {
  kNone         = 0x000,
  kBlendMode    = 0x001,
  kDataspace    = 0x002,
  kDisplayFrame = 0x004,
  kPlaneAlpha   = 0x008,
  kSourceCrop   = 0x010,
  kTransform    = 0x020,
  kZOrder       = 0x040,
  kAdded        = 0x080,
  kRemoved      = 0x100,
  kBufferGeometry = 0x200,
  kConfigChanged = 0x400,
};

class HWCLayer {
 public:
  explicit HWCLayer(Display display_id, HWCBufferAllocator *buf_allocator);
  ~HWCLayer();
  uint32_t GetZ() const { return z_; }
  LayerId GetId() const { return id_; }
  Layer *GetSDMLayer() { return layer_; }
  void ResetPerFrameData();

  HWC3::Error SetLayerBlendMode(BlendMode mode);
  HWC3::Error SetLayerBuffer(buffer_handle_t buffer, int32_t acquire_fence);
  HWC3::Error SetLayerColor(Color color);
  HWC3::Error SetLayerCompositionType(Composition type);
  HWC3::Error SetLayerDataspace(int32_t dataspace);
  HWC3::Error SetLayerDisplayFrame(Rect frame);
  HWC3::Error SetCursorPosition(int32_t x, int32_t y);
  HWC3::Error SetLayerPlaneAlpha(float alpha);
  HWC3::Error SetLayerSourceCrop(FRect crop);
  HWC3::Error SetLayerSurfaceDamage(Region damage);
  HWC3::Error SetLayerTransform(Transform transform);
  HWC3::Error SetLayerVisibleRegion(Region visible);
  HWC3::Error SetLayerPerFrameMetadata(uint32_t num_elements, const PerFrameMetadataKey *keys,
                                       const float *metadata);
  HWC3::Error SetLayerPerFrameMetadataBlobs(uint32_t num_elements, const PerFrameMetadataKey *keys,
                                            const uint32_t *sizes, const uint8_t* metadata);
  HWC3::Error SetLayerZOrder(uint32_t z);
  void SetComposition(const LayerComposition &sdm_composition);
  Composition GetClientRequestedCompositionType() { return client_requested_; }
  Composition GetOrigClientRequestedCompositionType() { return client_requested_orig_; }
  void UpdateClientCompositionType(Composition type) { client_requested_ = type; }
  Composition GetDeviceSelectedCompositionType() { return device_selected_; }
  int32_t GetLayerDataspace() { return dataspace_; }
  uint32_t GetGeometryChanges() { return geometry_changes_; }
  void ResetGeometryChanges() { geometry_changes_ = GeometryChanges::kNone; }
  void PushBackReleaseFence(int32_t fence);
  int32_t PopBackReleaseFence(void);
  int32_t PopFrontReleaseFence(void);
  void ResetValidation() { layer_->update_mask.reset(); }
  bool NeedsValidation() { return (geometry_changes_ || layer_->update_mask.any()); }
  bool IsSingleBuffered() { return single_buffer_; }
  bool IsScalingPresent();
  bool IsRotationPresent();
  bool IsDataSpaceSupported();
  static LayerBufferFormat GetSDMFormat(const int32_t &source, const int flags);
  bool IsSurfaceUpdated() { return surface_updated_; }
  void SetPartialUpdate(bool enabled) { partial_update_enabled_ = enabled; }
  bool IsNonIntegralSourceCrop() { return non_integral_source_crop_; }
  bool HasMetaDataRefreshRate() { return has_metadata_refresh_rate_; }
  void SetLayerAsMask();
  bool BufferLatched() { return buffer_flipped_; }
  void ResetBufferFlip() { buffer_flipped_ = false; }

 private:
  Layer *layer_ = nullptr;
  uint32_t z_ = 0;
  const LayerId id_;
  const Display display_id_;
  static std::atomic<LayerId> next_id_;
  std::deque<int32_t> release_fences_;
  HWCBufferAllocator *buffer_allocator_ = NULL;
  int32_t dataspace_ =  HAL_DATASPACE_UNKNOWN;
  LayerTransform layer_transform_ = {};
  LayerRect dst_rect_ = {};
  bool single_buffer_ = false;
  int buffer_fd_ = -1;
  bool dataspace_supported_ = false;
  bool partial_update_enabled_ = false;
  bool surface_updated_ = true;
  bool non_integral_source_crop_ = false;
  bool has_metadata_refresh_rate_ = false;
  bool buffer_flipped_ = false;

  // Composition requested by client(SF) Original
  Composition client_requested_orig_ = Composition::DEVICE;
  // Composition requested by client(SF) Modified for internel use
  Composition client_requested_ = Composition::DEVICE;
  // Composition selected by SDM
  Composition device_selected_ = Composition::DEVICE;
  uint32_t geometry_changes_ = GeometryChanges::kNone;

  void SetRect(const Rect &source, LayerRect *target);
  void SetRect(const FRect &source, LayerRect *target);
  uint32_t GetUint32Color(const Color &source);
  LayerBufferS3DFormat GetS3DFormat(uint32_t s3d_format);
  void GetUBWCStatsFromMetaData(UBWCStats *cr_stats, UbwcCrStatsVector *cr_vec);
  DisplayError SetMetaData(const private_handle_t *pvt_handle, Layer *layer);
  uint32_t RoundToStandardFPS(float fps);
  void ValidateAndSetCSC(const private_handle_t *handle);
  void SetDirtyRegions(Region surface_damage);
};

struct SortLayersByZ {
  bool operator()(const HWCLayer *lhs, const HWCLayer *rhs) const {
    return lhs->GetZ() < rhs->GetZ();
  }
};

}  // namespace sdm
#endif  // __HWC_LAYERS_H__
