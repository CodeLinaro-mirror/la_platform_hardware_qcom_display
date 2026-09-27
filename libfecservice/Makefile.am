# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

h_sources = IFECService.h

cpp_sources = IFECService.cpp

fecservice_includedir = $(pkgincludedir)
fecservice_include_HEADERS = $(h_sources)

lib_LTLIBRARIES = libfecservice.la
libfecservice_la_CC = @CC@
libfecservice_la_SOURCES = $(cpp_sources)
libfecservice_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"fecservice\"
libfecservice_la_CPPFLAGS = $(AM_CPPFLAGS) -DLOG_TAG=\"fecservice\"
libfecservice_la_LIBADD = -lutils -lcutils -llog -lbinder
libfecservice_la_LDFLAGS = -shared -avoid-version
