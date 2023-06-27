/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __GR_UBWCP_UTILS_H__
#define __GR_UBWCP_UTILS_H__

#include "gr_utils.h"
#ifdef TARGET_USES_UBWCP
#include "UBWCPLib.h"
#endif

namespace gralloc {

class UbwcpUtils {
 public:
  void ConfigUBWCPAttributes(private_handle_t const *handle);
  static UbwcpUtils *GetInstance();

 private:
  UbwcpUtils();
  ~UbwcpUtils();
  // link(s)to ubwcp library.
  void *(*LINK_UBWCPLib_create_session)(void);
  void (*LINK_UBWCPLib_destroy_session)(void *);
#ifdef TARGET_USES_UBWCP
  int (*LINK_UBWCPLib_get_stride_alignment)(void *, UBWCPLib_Image_Format, size_t *);
  int (*LINK_UBWCPLib_validate_stride)(void *, unsigned int, UBWCPLib_Image_Format, unsigned int);
  int (*LINK_UBWCPLib_set_buf_attrs)(void *, unsigned int, UBWCPLib_buf_attrs *);

  /*
   * Function to get the corresponding ubwcplib format for given HAL format
   */
  UBWCPLib_Image_Format GetUbwcpPixelFormat(int hal_format);
#endif

  /*
   * Function to check if stride is aligned
   */
  bool IsStrideAligned(int hal_format, int width);

  void *libUbwcpUtils_ = NULL;

  static UbwcpUtils *s_instance;
};

}  // namespace gralloc

#endif  // __GR_UBWCP_UTILSH__
