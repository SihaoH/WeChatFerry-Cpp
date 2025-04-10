find_package(spdlog CONFIG REQUIRED)
find_package(nng CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
set(WCF_DIR ${PROJECT_SOURCE_DIR}/wcf/WeChatFerry/WeChatFerry)

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
)
target_link_libraries(${TARGET} PRIVATE spdlog::spdlog_header_only)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8")

add_custom_command(
    TARGET ${TARGET} PRE_BUILD
    COMMAND ${WCF_DIR}/rpc/tool/protoc --nanopb_out=. wcf.proto
    DEPENDS ${WCF_DIR}/rpc/proto/wcf.proto
    WORKING_DIRECTORY ${WCF_DIR}/rpc/proto
    COMMENT "Generating PB files"
)


set(TARGET spy)
set(SOURCES
    ${WCF_DIR}/com/util.cpp
    ${WCF_DIR}/spy/account_manager.cpp
    ${WCF_DIR}/spy/chatroom_manager.cpp
    ${WCF_DIR}/spy/contact_manager.cpp
    ${WCF_DIR}/spy/database_executor.cpp
    ${WCF_DIR}/spy/dllmain.cpp
    ${WCF_DIR}/spy/message_handler.cpp
    ${WCF_DIR}/spy/message_sender.cpp
    ${WCF_DIR}/spy/misc_manager.cpp
    ${WCF_DIR}/spy/rpc_server.cpp
    ${WCF_DIR}/spy/spy.cpp
    ${WCF_DIR}/spy/spy.def
    ${WCF_DIR}/spy/spy.rc
    ${CMAKE_CURRENT_SOURCE_DIR}/codec_mock.cpp
)
set(HEADERS
    ${WCF_DIR}/com/log.hpp
    ${WCF_DIR}/com/util.h
    ${WCF_DIR}/smc/codec.h
    ${WCF_DIR}/spy/account_manager.h
    ${WCF_DIR}/spy/chatroom_manager.h
    ${WCF_DIR}/spy/contact_manager.h
    ${WCF_DIR}/spy/database_executor.h
    ${WCF_DIR}/spy/framework.h
    ${WCF_DIR}/spy/message_handler.h
    ${WCF_DIR}/spy/message_sender.h
    ${WCF_DIR}/spy/misc_manager.h
    ${WCF_DIR}/spy/offsets.h
    ${WCF_DIR}/spy/resource.h
    ${WCF_DIR}/spy/rpc_helper.h
    ${WCF_DIR}/spy/rpc_server.h
    ${WCF_DIR}/spy/spy.h
    ${WCF_DIR}/spy/spy_types.h
    ${WCF_DIR}/spy/sqlite3.h
)
add_library(${TARGET} SHARED ${SOURCES} ${HEADERS})
target_link_libraries(${TARGET}
    PRIVATE
    nnrpc
    spdlog::spdlog_header_only
    minhook::minhook
    nng::nng
)
target_include_directories(${TARGET}
    PRIVATE 
    ${WCF_DIR}/smc
    ${WCF_DIR}/spy
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8")
target_compile_definitions(${TARGET} PRIVATE "WIN32;WSPY_EXPORTS;_WINDOWS;_USRDLL;_UNICODE")
target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS")

# DEBUG版本下生成的dll名称带_debug后缀
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}$<$<CONFIG:Debug>:_debug>)


set(TARGET sdk)
set(SOURCES
    ${WCF_DIR}/com/util.h
    ${WCF_DIR}/com/log.hpp
    ${WCF_DIR}/sdk/framework.h
    ${WCF_DIR}/sdk/injector.h
    ${WCF_DIR}/sdk/sdk.h
)
set(HEADERS
    ${WCF_DIR}/com/util.cpp
    ${WCF_DIR}/sdk/dllmain.cpp
    ${WCF_DIR}/sdk/injector.cpp
    ${WCF_DIR}/sdk/sdk.cpp
    ${WCF_DIR}/sdk/sdk.def
)
add_library(${TARGET} SHARED ${SOURCES} ${HEADERS})
target_link_libraries(${TARGET} PRIVATE spdlog::spdlog_header_only)
target_include_directories(${TARGET}
    PUBLIC
    ${WCF_DIR}/sdk
    PRIVATE
    ${WCF_DIR}/spy
    ${WCF_DIR}/com
    ${VCPKG_INCLUDE_DIR}
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8")
target_compile_definitions(${TARGET} PRIVATE "WIN32;NDEBUG;SDK_EXPORTS;_WINDOWS;_USRDLL;_UNICODE")
target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS")
add_custom_command( #拷贝免责声明
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${WCF_DIR}/DISCLAIMER.md ${CMAKE_OUTPUT_DIRECTORY_INTDIR}/DISCLAIMER.md
    DEPENDS ${WCF_DIR}/DISCLAIMER.md
)
