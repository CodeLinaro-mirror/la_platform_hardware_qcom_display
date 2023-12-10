/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __HWC_PARCEL_H__
#define __HWC_PARCEL_H__

#include <binder/Parcel.h>

#include "sdm_display_intf_parcel.h"

namespace sdm {

class HWCParcel : public SDMParcel {
 public:
  explicit HWCParcel(const ::android::Parcel *parcel) {
    parcel_ = const_cast<::android::Parcel *>(parcel);
  }

  virtual ~HWCParcel() {}

  uint32_t readInt32() override { return parcel_->readInt32(); }

  uint64_t readInt64() override { return parcel_->readInt64(); }

  float readFloat() override { return parcel_->readFloat(); }

  void writeInt32(uint32_t value) override { parcel_->writeInt32(value); }

  uint32_t dataSize() override { return parcel_->dataSize(); }

  uint32_t writeFloat(float val) override { return parcel_->writeFloat(val); }

  uint32_t dataPosition() override { return parcel_->dataPosition(); }

  uint32_t writeUint64(uint64_t value) override { return parcel_->writeUint64(value); }

  uint32_t dataAvail() override { return parcel_->dataAvail(); }

  const void *readInplace(uint32_t size) override { return parcel_->readInplace(size); }

  uint32_t write(const void *data, uint32_t len) override { return parcel_->write(data, len); }

  uint32_t writeDupFileDescriptor(int fd) override { return parcel_->writeDupFileDescriptor(fd); }

  double readDouble() override { return parcel_->readDouble(); }

 private:
  ::android::Parcel *parcel_ = nullptr;
};
}  //namespace sdm

#endif  // __HWC_PARCEL_H__
