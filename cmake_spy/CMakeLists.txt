set(TARGET spy)

find_package(fmt REQUIRED)
find_package(nng REQUIRED)
find_package(minhook REQUIRED)

set(NNRPC_SOURCES
    ${WCF_DIR}/rpc/nanopb/pb.h
    ${WCF_DIR}/rpc/nanopb/pb_common.c
    ${WCF_DIR}/rpc/nanopb/pb_common.h
    ${WCF_DIR}/rpc/nanopb/pb_decode.c
    ${WCF_DIR}/rpc/nanopb/pb_decode.h
    ${WCF_DIR}/rpc/nanopb/pb_encode.c
    ${WCF_DIR}/rpc/nanopb/pb_encode.h
    ${WCF_DIR}/rpc/pb_types.h
    ${WCF_DIR}/rpc/pb_util.cpp
    ${WCF_DIR}/rpc/pb_util.h
    ${WCF_DIR}/rpc/proto/wcf.pb.c
    ${WCF_DIR}/rpc/proto/wcf.pb.h
    ${WCF_DIR}/rpc/proto/wcf.proto
)
set_source_files_properties(${WCF_DIR}/rpc/proto/wcf.pb.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${WCF_DIR}/rpc/proto/wcf.pb.h PROPERTIES GENERATED TRUE)
add_library(nnrpc STATIC ${NNRPC_SOURCES})
target_include_directories(nnrpc
    PUBLIC
    ${WCF_DIR}/com
    ${WCF_DIR}/rpc
    ${WCF_DIR}/rpc/nanopb
    ${WCF_DIR}/rpc/proto
    ${VCPKG_INCLUDE_DIR}
)
target_compile_options(nnrpc PRIVATE "/permissive-" "/EHa" "/utf-8" "/MT$<$<CONFIG:Debug>:d>")

add_custom_command(
    TARGET nnrpc PRE_BUILD
    COMMAND ${WCF_DIR}/rpc/tool/protoc --nanopb_out=. wcf.proto
    DEPENDS ${WCF_DIR}/rpc/proto/wcf.proto
    WORKING_DIRECTORY ${WCF_DIR}/rpc/proto
    COMMENT "Generating PB files"
)

set(SPY_SOURCES
    ${WCF_DIR}/com/log.cpp
    ${WCF_DIR}/com/util.cpp
    ${WCF_DIR}/rpc/nanopb/pb_common.c
    ${WCF_DIR}/rpc/nanopb/pb_decode.c
    ${WCF_DIR}/rpc/nanopb/pb_encode.c
    ${WCF_DIR}/rpc/pb_util.cpp
    ${WCF_DIR}/rpc/proto/wcf.pb.c
    ${WCF_DIR}/spy/chatroom_mgmt.cpp
    ${WCF_DIR}/spy/funcs.cpp
    ${WCF_DIR}/spy/dllmain.cpp
    ${WCF_DIR}/spy/exec_sql.cpp
    ${WCF_DIR}/spy/contact_mgmt.cpp
    ${WCF_DIR}/spy/receive_msg.cpp
    ${WCF_DIR}/spy/rpc_server.cpp
    ${WCF_DIR}/spy/send_msg.cpp
    ${WCF_DIR}/spy/spy.cpp
    ${WCF_DIR}/spy/user_info.cpp
    ${WCF_DIR}/spy/spy.def
)

set(SPY_HEADERS
    ${WCF_DIR}/com/log.h
    ${WCF_DIR}/com/util.h
    ${WCF_DIR}/smc/codec.h
    ${WCF_DIR}/spy/chatroom_mgmt.h
    ${WCF_DIR}/spy/funcs.h
    ${WCF_DIR}/spy/exec_sql.h
    ${WCF_DIR}/spy/framework.h
    ${WCF_DIR}/spy/contact_mgmt.h
    ${WCF_DIR}/spy/receive_msg.h
    ${WCF_DIR}/spy/resource.h
    ${WCF_DIR}/spy/rpc_server.h
    ${WCF_DIR}/spy/send_msg.h
    ${WCF_DIR}/spy/spy.h
    ${WCF_DIR}/spy/spy_types.h
    ${WCF_DIR}/spy/sqlite3.h
    ${WCF_DIR}/spy/user_info.h
)

add_library(${TARGET} SHARED ${SPY_SOURCES} ${SPY_HEADERS})
target_link_libraries(${TARGET}
    PRIVATE
    smc
    nnrpc
    fmt::fmt
    minhook::minhook
    nng::nng
)
target_include_directories(${TARGET}
    PRIVATE 
    ${WCF_DIR}/spy
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8" "/MT$<$<CONFIG:Debug>:d>")
target_compile_definitions(${TARGET} PRIVATE "WIN32;WSPY_EXPORTS;_WINDOWS;_USRDLL;_UNICODE")
target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS")

# DEBUG版本下生成的dll名称带_debug后缀
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}$<$<CONFIG:Debug>:_debug>)

