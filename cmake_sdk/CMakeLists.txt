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
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${TARGET}
    PRIVATE
    spdlog::spdlog_header_only
)
target_include_directories(${TARGET}
    PUBLIC
    ${WCF_DIR}/sdk
    PRIVATE
    ${WCF_DIR}/spy
    ${WCF_DIR}/com
    ${VCPKG_INCLUDE_DIR}
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8" "/MD$<$<CONFIG:Debug>:d>")
target_compile_definitions(${TARGET} PRIVATE "WIN32;NDEBUG;SDK_EXPORTS;_WINDOWS;_USRDLL;_UNICODE")
target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS")

# øΩ±¥≈‰÷√Œƒº˛
add_custom_command(
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/WeChatFerry/WeChatFerry/DISCLAIMER.md ${CMAKE_OUTPUT_DIRECTORY_INTDIR}/DISCLAIMER.md
    DEPENDS ${PROJECT_SOURCE_DIR}/WeChatFerry/WeChatFerry/DISCLAIMER.md
)
