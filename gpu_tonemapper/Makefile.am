HEADER_PATH := ${WORKSPACE}/hardware/qcom/display/gpu_tonemapper

cpp_sources = TonemapFactory.cpp \
            glengine.cpp \
            EGLImageBuffer.cpp \
            EGLImageWrapper.cpp \
            Tonemapper.cpp
h_sources = $(HEADER_PATH)/*.h

include_HEADERS = $(h_sources)

lib_LTLIBRARIES = libgpu_tonemapper.la
libgpu_tonemapper_la_CC = @CC@
libgpu_tonemapper_la_SOURCES = $(cpp_sources)
libgpu_tonemapper_la_CFLAGS = $(COMMON_CFLAGS) -Wno-missing-field-initializers -Wall \
                              -Wno-unused-parameter -DLOG_TAG=\"GPU_TONEMAPPER\"
libgpu_tonemapper_la_CPPFLAGS = $(AM_CPPFLAGS)
libgpu_tonemapper_la_LDFLAGS = -shared -avoid-version