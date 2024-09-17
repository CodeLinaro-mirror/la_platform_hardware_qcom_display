%.pb.cc %.pb.h %.grpc.pb.cc %.grpc.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) \
	--cpp_out=$(srcdir) --grpc_out=$(srcdir) $^

AM_CPPFLAGS = -I./

nodist_proto_SOURCES =  qdcm_service.grpc.pb.cc \
                        qdcm_service.pb.cc

BUILT_SOURCES = qdcm_service.grpc.pb.h \
                qdcm_service.pb.h


sources_libqdcmproto = $(nodist_proto_SOURCES)

qdcmproto_includedir = $(pkgincludedir)/display
qdcmproto_include_HEADERS = $(BUILT_SOURCES)

lib_LTLIBRARIES = libqdcmproto.la

requiredlibs_qdcmproto = -lgrpc++ -lprotobuf

libqdcmproto_la_CC = @CC@
libqdcmproto_la_SOURCES = $(sources_libqdcmproto)
libqdcmproto_la_CFLAGS = $(AM_CFLAGS) @PROTOBUF_CFLAGS@ @GRPC_CFLAGS@
libqdcmproto_la_LDFLAGS = -shared -avoid-version @LDFLAGS@ -lgrpc++
libqdcmproto_la_LIBADD = $(requiredlibs_qdcmproto)