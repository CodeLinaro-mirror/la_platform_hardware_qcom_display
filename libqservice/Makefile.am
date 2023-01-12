AM_CFLAGS    = -Wall -Werror -Wno-sign-conversion -Wno-missing-field-initializers \
               -Wno-unused-parameter

h_sources = IQService.h \
            IQClient.h \
            QService.h \
            QServiceUtils.h \
            IQHDMIClient.h

cpp_sources = QService.cpp \
              IQService.cpp \
              IQClient.cpp \
              IQHDMIClient.cpp

qservice_includedir = $(pkgincludedir)/display
qservice_include_HEADERS = $(h_sources)

AM_CPPFLAGS = "-I${WORKSPACE}/system/core/include/private"

lib_LTLIBRARIES = libqservice.la
libqservice_la_CC = @CC@
libqservice_la_SOURCES = $(cpp_sources)
libqservice_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"qdqservice\"
libqservice_la_CPPFLAGS = $(AM_CPPFLAGS)
libqservice_la_LIBADD = -lhardware -lcutils -llog -lbinder
libqservice_la_LDFLAGS = -shared -avoid-version