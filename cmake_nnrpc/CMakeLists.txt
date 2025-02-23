set(TARGET nnrpc)

set(SOURCES
    ${WCF_DIR}/rpc/nanopb/pb_common.c
    ${WCF_DIR}/rpc/nanopb/pb_decode.c
    ${WCF_DIR}/rpc/nanopb/pb_encode.c
    ${WCF_DIR}/rpc/pb_util.cpp
    ${WCF_DIR}/rpc/proto/wcf.pb.c
    ${WCF_DIR}/rpc/proto/wcf.proto
)
set(HEADERS
    ${WCF_DIR}/rpc/nanopb/pb.h
    ${WCF_DIR}/rpc/nanopb/pb_common.h
    ${WCF_DIR}/rpc/nanopb/pb_decode.h
    ${WCF_DIR}/rpc/nanopb/pb_encode.h
    ${WCF_DIR}/rpc/pb_types.h
    ${WCF_DIR}/rpc/pb_util.h
    ${WCF_DIR}/rpc/proto/wcf.pb.h
)
set_source_files_properties(${WCF_DIR}/rpc/proto/wcf.pb.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${WCF_DIR}/rpc/proto/wcf.pb.h PROPERTIES GENERATED TRUE)

add_library(${TARGET} STATIC ${SOURCES} ${HEADERS})
target_include_directories(${TARGET}
    PUBLIC
    ${WCF_DIR}/com
    ${WCF_DIR}/rpc
    ${WCF_DIR}/rpc/nanopb
    ${WCF_DIR}/rpc/proto
    ${VCPKG_INCLUDE_DIR}
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8" "/MD$<$<CONFIG:Debug>:d>")

add_custom_command(
    TARGET ${TARGET} PRE_BUILD
    COMMAND ${WCF_DIR}/rpc/tool/protoc --nanopb_out=. wcf.proto
    DEPENDS ${WCF_DIR}/rpc/proto/wcf.proto
    WORKING_DIRECTORY ${WCF_DIR}/rpc/proto
    COMMENT "Generating PB files"
)
