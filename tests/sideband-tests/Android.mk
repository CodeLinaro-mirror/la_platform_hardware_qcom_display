LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE           := sideband-test
LOCAL_MODULE_TAGS      := tests
LOCAL_CFLAGS           := -DLOG_TAG=\"SidebandTest\"  -fstack-protector-all \
                          -g  -Wall -Wextra -Werror -fno-builtin -Wno-unused-parameter -Wno-unused-variable

LOCAL_SHARED_LIBRARIES := libcutils libutils liblog libhidlbase libsync \
                          libgrallocutils \
                          android.hardware.graphics.allocator@2.0 \
                          android.hardware.graphics.mapper@2.0 \
                          android.hardware.graphics.common@1.1 \
                          vendor.display.config@1.21

LOCAL_HEADER_LIBRARIES := display_headers
LOCAL_SRC_FILES        := sideband_test.cpp
LOCAL_VENDOR_MODULE    := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_NATIVE_TESTS)/display-tests

include $(BUILD_EXECUTABLE)
