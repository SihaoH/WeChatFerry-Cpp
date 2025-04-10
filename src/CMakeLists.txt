set(TARGET app)

set(CMAKE_AUTOMOC ON)

set(SOURCES
    main.cpp
    Application.h
    Application.cpp
    Client.h
    Client.cpp
    ChatRobot.h
    ChatRobot.cpp
    DataUtil.h
    DataUtil.cpp
    Logger.h
    Logger.cpp
    NngSocket.h
    NngSocket.cpp
)
add_executable(${TARGET} ${SOURCES})
find_package(spdlog CONFIG REQUIRED)
find_package(nng CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(${TARGET}
    PRIVATE
    sdk
    nnrpc
    spdlog::spdlog_header_only
    nng::nng
    httplib::httplib
    Qt6::Core
)
target_compile_options(${TARGET} PRIVATE "/utf-8" "/MD$<$<CONFIG:Debug>:d>")
target_compile_definitions(${TARGET} PRIVATE "_UNICODE")

# 修改从VS启动时的工作目录路径
set_target_properties(${TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_OUTPUT_DIRECTORY_INTDIR})

# 为config.json创建符号链接
add_custom_command(
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_OUTPUT_DIRECTORY_INTDIR}/config.json
)
