set(TARGET sdk)

find_package(fmt REQUIRED)

set(SOURCES
    ${WCF_DIR}/com/util.h
    ${WCF_DIR}/com/log.h
    ${WCF_DIR}/sdk/framework.h
    ${WCF_DIR}/sdk/injector.h
    ${WCF_DIR}/sdk/sdk.h
)

set(HEADERS
    ${WCF_DIR}/com/util.cpp
    ${WCF_DIR}/com/log.cpp
    ${WCF_DIR}/sdk/dllmain.cpp
    ${WCF_DIR}/sdk/injector.cpp
    ${WCF_DIR}/sdk/sdk.cpp
    ${WCF_DIR}/sdk/sdk.def
)

add_library(${TARGET} SHARED ${SOURCES} ${HEADERS})
add_dependencies(${TARGET} spy)
target_link_libraries(${TARGET}
    PRIVATE
    fmt::fmt
)
target_include_directories(${TARGET}
    PUBLIC
    ${WCF_DIR}/sdk
    PRIVATE
    ${WCF_DIR}/spy
    ${WCF_DIR}/com
    ${VCPKG_INCLUDE_DIR}
)
target_compile_options(${TARGET} PRIVATE "/permissive-" "/EHa" "/utf-8" "/MT$<$<CONFIG:Debug>:d>")
target_compile_definitions(${TARGET} PRIVATE "WIN32;NDEBUG;SDK_EXPORTS;_WINDOWS;_USRDLL;_UNICODE")
target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS")
