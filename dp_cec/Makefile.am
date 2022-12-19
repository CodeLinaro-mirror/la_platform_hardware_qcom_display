AM_CFLAGS = -Wall -Wundef -Wno-trigraphs -g -O3 "-std=c++11" -include stdint.h

AM_CPPFLAGS = $(CPPFLAGS)
cpp_sources = qdp_cec.cpp
h_sources = qdp_cec.h

lib_LTLIBRARIES = libdp_cec.la
libdp_cec_la_CC = @CC@
libdp_cec_la_SOURCES = $(cpp_sources) $(h_sources)
libdp_cec_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"qddp_cec\"
libdp_cec_la_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) -fPIC
libdp_cec_la_LDFLAGS = -shared -module -avoid-version
