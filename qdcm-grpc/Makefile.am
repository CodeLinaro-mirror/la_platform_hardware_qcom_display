ACLOCAL_AMFLAGS = -I m4

dirs = proto
SUBDIRS = $(dirs)

h_sources = qdcm_grpc_server.h \
            qdcm_service_intf.h \
            qdcm_service_impl.h

cpp_sources = qdcm_grpc_server.cpp \
              qdcm_service_impl.cpp

qdcmgrpc_includedir = $(pkgincludedir)/display
qdcmgrpc_include_HEADERS = $(h_sources)

lib_LTLIBRARIES = libqdcmgrpc.la
libqdcmgrpc_la_CC = @CC@
libqdcmgrpc_la_SOURCES = $(cpp_sources)
libqdcmgrpc_la_CFLAGS = $(COMMON_CFLAGS) @PROTOBUF_CFLAGS@ @GRPC_CFLAGS@ \
                                                    -DLOG_TAG=\"qdcmgrpc\"
libqdcmgrpc_la_CPPFLAGS = $(AM_CPPFLAGS)
libqdcmgrpc_la_CPPFLAGS += -I$(srcdir)/../libdebug -I$(srcdir)/proto
libqdcmgrpc_la_LIBADD = ../libdebug/libdisplaydebug.la ./proto/libqdcmproto.la
libqdcmgrpc_la_LDFLAGS = -shared -avoid-version -lgrpc++ -lprotobuf
