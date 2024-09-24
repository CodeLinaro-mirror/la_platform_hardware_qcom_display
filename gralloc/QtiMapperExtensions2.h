/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QTIMAPPEREXTENSIONS2_H__
#define __QTIMAPPEREXTENSIONS2_H__

#include <vendor/qti/hardware/display/mapperextensions/IQtiMapperExt.h>
#include <vendor/qti/hardware/display/mapperextensions/IQtiMapperExtProvider.h>

#include "gr_snap_helper.h"

namespace stablec {
namespace vendor {
namespace qti {
namespace hardware {
namespace display {
namespace mapperextensions2 {

using Error = IQtiMapperExt_Error;
using SnapMetadataType = vendor_qti_hardware_display_common_MetadataType;

class QtiMapperExtensions2 final : public ::vendor::qtimapperext::IQtiMapperExtV2Impl {
 public:
  QtiMapperExtensions2();
  ~QtiMapperExtensions2() override = default;

  Error getMultiViewInfo(buffer_handle_t _Nonnull buffer, uint32_t *_Nonnull views) override;

  Error getBaseView(buffer_handle_t _Nonnull buffer, uint32_t *_Nonnull view) override;

  Error importViewBuffer(buffer_handle_t _Nonnull metaHandle, uint32_t view,
                         buffer_handle_t _Nullable *_Nonnull outBufferHandle) override;

 private:
  gralloc::GrallocSnapHelper *_Nullable snap_helper_ = nullptr;
  bool snap_alloc_enable_ = false;
};

}  // namespace mapperextensions2
}  // namespace display
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  // namespace stablec

#endif  // __QTIMAPPEREXTENSIONS2_H__
