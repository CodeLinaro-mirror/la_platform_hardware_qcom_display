AM_CFLAGS    = -Wno-missing-field-initializers -Wconversion \
               -std=c++17 -DLOG_TAG=\"SDE_DRM\" -DUNIX_OS


cpp_sources  = drm_manager.cpp \
               drm_connector.cpp \
               drm_crtc.cpp \
               drm_plane.cpp \
               drm_encoder.cpp \
               drm_atomic_req.cpp \
               drm_utils.cpp \
               drm_pp_manager.cpp \
               drm_property.cpp \
               drm_dpps_mgr_imp.cpp \
               drm_panel_feature_mgr.cpp


lib_LTLIBRARIES = libsdedrm.la
libsdedrm_la_CC = @CC@
libsdedrm_la_SOURCES = $(cpp_sources)
libsdedrm_la_CFLAGS = $(AM_CFLAGS) -DLOG_TAG=\"SDE_DRM\"
libsdedrm_la_CPPFLAGS = $(AM_CPPFLAGS) -DPP_DRM_ENABLE
libsdedrm_la_LIBADD = ../libdrmutils/libdrmutils.la ../libdebug/libdisplaydebug.la
libsdedrm_la_LDFLAGS = -shared -version-number @L_VERSION_NUMBER@ -ldrm
libsdedrm_la_CPPFLAGS += -I$(srcdir)/../libdebug
libsdedrm_la_CPPFLAGS += -I$(srcdir)/../libdrmutils
libsdedrm_la_CPPFLAGS +=  @LIBDRM_CFLAGS@
libsdedrm_la_CPPFLAGS +=  -I$(srcdir)/../include
