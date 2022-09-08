/*
* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted
* provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright notice, this list of
*      conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright notice, this list of
*      conditions and the following disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written
*      permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#ifndef __SDM_COMP_TYPES_H__
#define __SDM_COMP_TYPES_H__

#include "core/layer_buffer.h"

namespace sdm {

typedef void * Handle;

struct Rect {
  float left = 0.0f;            //!< Specifies the left coordinates of the pixel buffer
  float top = 0.0f;             //!< Specifies the top coordinates of the pixel buffer
  float right = 0.0f;           //!< Specifies the right coordinates of the pixel buffer
  float bottom = 0.0f;          //!< Specifies the bottom coordinates of the pixel buffer
};

struct BufferHandle {
  int32_t fd = -1;                             //!< fd of the allocated buffer to be displayed.
  shared_ptr<Fence> producer_fence = nullptr;  //!< Created and signaled by the producer. Consumer
                                               //!< needs to wait on this before the buffer
                                               //!< is being accessed
  shared_ptr<Fence> consumer_fence = nullptr;  //!< Created and signaled by the consumer. Producer
                                               //!< needs to wait on this before the buffer
                                               //!< is being modified
  uint32_t width = 0;                          //!< Actual width of the buffer in pixels
  uint32_t height = 0;                         //!< Actual height of the buffer in pixels.
  uint32_t aligned_width = 0;                  //!< Aligned width of the buffer in pixels
  uint32_t aligned_height = 0;                 //!< Aligned height of the buffer in pixels
  LayerBufferFormat format = kFormatInvalid;   //!< Format of the buffer refer BufferFormat
  uint32_t stride_in_bytes = 0;                //!< Stride of the buffer in bytes
  uint32_t size = 0;                           //!< Allocated buffer size
  bool uncached = false;                       //!< Enable or disable buffer caching during R/W
  int64_t buffer_id = -1;                      //!< Unique Id of the allocated buffer for the
                                               //!< internal use only
  Rect src_crop = {};                          //!< Crop rectangle of src buffer, if client doesn't
                                               //!< specify, its default to {0, 0, width, height}
};

enum SDMCompDisplayType {
  kSDMCompDisplayTypePrimary,       // Defines the display type for primary display
  kSDMCompDisplayTypeSecondary1,    // Defines the display type for secondary builtin display
  kSDMCompDisplayTypeMax,
};

// The following values matches the HEVC spec
typedef enum ColorPrimaries {
  // Unused = 0;
  ColorPrimaries_BT709_5     = 1,  // ITU-R BT.709-5 or equivalent
  /* Unspecified = 2, Reserved = 3*/
  ColorPrimaries_BT470_6M    = 4,  // ITU-R BT.470-6 System M or equivalent
  ColorPrimaries_BT601_6_625 = 5,  // ITU-R BT.601-6 625 or equivalent
  ColorPrimaries_BT601_6_525 = 6,  // ITU-R BT.601-6 525 or equivalent
  ColorPrimaries_SMPTE_240M  = 7,  // SMPTE_240M
  ColorPrimaries_GenericFilm = 8,  // Generic Film
  ColorPrimaries_BT2020      = 9,  // ITU-R BT.2020 or equivalent
  ColorPrimaries_SMPTE_ST428 = 10,  // SMPTE_240M
  ColorPrimaries_AdobeRGB    = 11,
  ColorPrimaries_DCIP3       = 12,
  ColorPrimaries_EBU3213     = 22,
  ColorPrimaries_Max         = 0xff,
} ColorPrimaries;

typedef enum GammaTransfer {
  // Unused = 0;
  Transfer_sRGB            = 1,  // ITR-BT.709-5
  /* Unspecified = 2, Reserved = 3 */
  Transfer_Gamma2_2        = 4,
  Transfer_Gamma2_8        = 5,
  Transfer_SMPTE_170M      = 6,  // BT.601-6 525 or 625
  Transfer_SMPTE_240M      = 7,  // SMPTE_240M
  Transfer_Linear          = 8,
  Transfer_Log             = 9,
  Transfer_Log_Sqrt        = 10,
  Transfer_XvYCC           = 11,  // IEC 61966-2-4
  Transfer_BT1361          = 12,  // Rec.ITU-R BT.1361 extended gamut
  Transfer_sYCC            = 13,  // IEC 61966-2-1 sRGB or sYCC
  Transfer_BT2020_2_1      = 14,  // Rec. ITU-R BT.2020-2 (same as the values 1, 6, and 15)
  Transfer_BT2020_2_2      = 15,  // Rec. ITU-R BT.2020-2 (same as the values 1, 6, and 14)
  Transfer_SMPTE_ST2084    = 16,  // 2084
  Transfer_ST_428          = 17,  // SMPTE ST 428-1
  Transfer_HLG             = 18,  // ARIB STD-B67
  Transfer_Max             = 0xff,
} GammaTransfer;

enum RenderIntent {
  //<! Colors with vendor defined gamut
  kRenderIntentNative,
  //<! Colors with in gamut are left untouched, out side the gamut are hard clipped
  kRenderIntentColorimetric,
  //<! Colors with in gamut are ehanced, out side the gamuat are hard clipped
  kRenderIntentEnhance,
  //<! Tone map hdr colors to display's dynamic range, mapping to display gamut is
  //<! defined in colormertic.
  kRenderIntentToneMapColorimetric,
  //<! Tone map hdr colors to display's dynamic range, mapping to display gamut is
  //<! defined in enhance.
  kRenderIntentToneMapEnhance,
  //<! Custom render intents range
  kRenderIntentOemCustomStart = 0x100,
  kRenderIntentOemCustomEnd = 0x1ff,
  //<! If STC implementation returns kOemModulateHw render intent, STC manager will
  //<! call the implementation for all the render intent/blend space combination.
  //<! STC implementation can modify/modulate the HW assets.
  kRenderIntentOemModulateHw = 0xffff - 1,
  kRenderIntentMaxRenderIntent = 0xffff
};

struct SDMCompDisplayAttributes {
  uint32_t vsync_period = 0;  //!< VSync period in nanoseconds.
  uint32_t x_res = 0;         //!< Total number of pixels in X-direction on the display panel.
  uint32_t y_res = 0;         //!< Total number of pixels in Y-direction on the display panel.
  float x_dpi = 0.0f;         //!< Dots per inch in X-direction.
  float y_dpi = 0.0f;         //!< Dots per inch in Y-direction.
  bool is_yuv = false;        //!< If the display output is in YUV format.
  uint32_t fps = 0;           //!< fps of the display.
  bool smart_panel = false;   //!< Speficies the panel is video mode or command mode
};

struct ColorMode {
  //<! Blend-Space gamut
  ColorPrimaries gamut = ColorPrimaries_Max;
  //<! Blend-space Gamma
  GammaTransfer gamma = Transfer_Max;
  //<! Intent of the mode
  RenderIntent intent = kRenderIntentMaxRenderIntent ;
};

}  // namespace sdm

#endif  // __SDM_COMP_TYPES_H__
