cmake_minimum_required(VERSION 3.30)

# 配置vcpkg的相关内容
set(VCPKG_TARGET_TRIPLET "x64-windows-static-md")
set(VCPKG_LIBRARY_LINKAGE static)

find_program(VCPKG_ROOT vcpkg.exe PATHS $ENV{PATH})
cmake_path(GET VCPKG_ROOT PARENT_PATH VCPKG_ROOT)
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

set(PROJ_NAME WeChatFerry-Cpp)
project(${PROJ_NAME} VERSION 0.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_OUTPUT_DIRECTORY_INTDIR ${PROJECT_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR})

set(WCF_DIR ${PROJECT_SOURCE_DIR}/WeChatFerry/WeChatFerry)
set(VCPKG_INCLUDE_DIR ${PROJECT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include)
add_subdirectory(cmake_smc)
add_subdirectory(cmake_nnrpc)
add_subdirectory(cmake_spy)
add_subdirectory(cmake_sdk)
add_subdirectory(src)
