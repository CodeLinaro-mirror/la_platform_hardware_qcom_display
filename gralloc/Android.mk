# Gralloc module
LOCAL_PATH := $(call my-dir)
include $(LOCAL_PATH)/../common.mk
include $(LIBION_HEADER_PATH_WRAPPER)
include $(CLEAR_VARS)

LOCAL_MODULE                  := gralloc.$(TARGET_BOARD_PLATFORM)
LOCAL_VENDOR_MODULE           := true
LOCAL_MODULE_RELATIVE_PATH    := hw
LOCAL_MODULE_TAGS             := optional
LOCAL_C_INCLUDES              := $(common_includes) $(kernel_includes)

LOCAL_HEADER_LIBRARIES        := display_headers
LOCAL_SHARED_LIBRARIES        := $(common_libs) libqdMetaData libsync libgrallocutils \
                                 libgralloccore \
                                 android.hardware.graphics.mapper@2.0 \
                                 android.hardware.graphics.mapper@2.1 \
                                 android.hardware.graphics.mapper@3.0 \
                                 android.hardware.graphics.mapper@4.0
LOCAL_CFLAGS                  := $(common_flags) -DLOG_TAG=\"qdgralloc\" -Wall -Werror \
                                 -D__QTI_DISPLAY_GRALLOC__ -D__MIN_ANDROID_VER_T__
LOCAL_CLANG                   := true
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_deps)
LOCAL_SRC_FILES               := gr_device_impl.cpp
include $(BUILD_SHARED_LIBRARY)
