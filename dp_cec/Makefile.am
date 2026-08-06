AM_CFLAGS = -Wall -Wundef -Wno-trigraphs -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32 -g -O3 "-std=c++17" -include stdint.h

AM_CPPFLAGS = $(CPPFLAGS)

cpp_sources = qdp_cec.cpp

h_sources = qdp_cec.h

lib_LTLIBRARIES = hdmi_cec.pluggable.la
hdmi_cec_pluggable_la_CC = @CC@
hdmi_cec_pluggable_la_SOURCES = $(cpp_sources) $(h_sources)
hdmi_cec_pluggable_la_CFLAGS = $(COMMON_CFLAGS) -DLOG_TAG=\"qddp_cec\"
hdmi_cec_pluggable_la_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) -fPIC
hdmi_cec_pluggable_la_LDFLAGS = -shared -module -avoid-version
