c_sources = core_interface.cpp \
            core_impl.cpp \
            display_base.cpp \
            display_builtin.cpp \
            noise_plugin_intf_impl.cpp \
            display_pluggable.cpp \
            display_virtual.cpp \
            display_null.cpp \
            comp_manager.cpp \
            strategy.cpp \
            resource_default.cpp \
            color_manager.cpp \
            hw_info_default.cpp

core_h_sources = $(srcdir)/../../include/core/*.h

core_includedir = $(includedir)/core
core_include_HEADERS = $(core_h_sources)

private_h_sources = $(srcdir)/../../include/private/*.h

private_includedir = $(includedir)/private
private_include_HEADERS = $(private_h_sources)

utils_h_sources = $(srcdir)/../../include/utils/*.h

utils_includedir = $(includedir)/utils
utils_include_HEADERS = $(utils_h_sources)

inc_includedir = $(includedir)
inc_include_HEADERS = $(srcdir)/../../../include/*.h

lib_LTLIBRARIES = libsdmcore.la
libsdmcore_la_CC = @CC@
libsdmcore_la_SOURCES = $(c_sources)
libsdmcore_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"SDM\"
libsdmcore_la_CPPFLAGS = $(AM_CPPFLAGS) -DPP_DRM_ENABLE
libsdmcore_la_CPPFLAGS += -I$(srcdir)/../../include
libsdmcore_la_CPPFLAGS += -I$(srcdir)/../../../libdebug
libsdmcore_la_CPPFLAGS += -I$(srcdir)/../../../include
libsdmcore_la_CPPFLAGS += -I$(srcdir)/../../../libdrmutils
libsdmcore_la_CPPFLAGS +=  @LIBDRM_CFLAGS@
libsdmcore_la_LIBADD = ../utils/libsdmutils.la ../dal/libsdmdal.la 
libsdmcore_la_LIBADD += ../../../libdebug/libdisplaydebug.la
libsdmcore_la_LDFLAGS = -shared -avoid-version -ldrm -ldl
