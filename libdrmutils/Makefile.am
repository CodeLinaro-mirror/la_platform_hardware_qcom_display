cpp_sources = drm_lib_loader.cpp \
              drm_master.cpp \
              drm_res_mgr.cpp

drmutils_h_sources = $(srcdir)/*.h
drmutils_includedir = $(includedir)
drmutils_include_HEADERS = $(drmutils_h_sources)

lib_LTLIBRARIES = libdrmutils.la
libdrmutils_la_CC = @CC@
libdrmutils_la_SOURCES = $(cpp_sources)
libdrmutils_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"DRMUTILS\"
libdrmutils_la_CPPFLAGS = $(AM_CPPFLAGS)
libdrmutils_la_CPPFLAGS +=  @LIBDRM_CFLAGS@
libdrmutils_la_CPPFLAGS +=  -I$(srcdir)/../include
libdrmutils_la_CPPFLAGS +=  -I$(srcdir)/../libdebug
libsdmcore_la_LIBADD = -ldrm
libdrmutils_la_LDFLAGS = -shared -version-number @L_VERSION_NUMBER@