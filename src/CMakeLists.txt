set(TARGET app)

find_package(fmt REQUIRED)
find_package(nng REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='"$(notdir $<)'"")

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
add_executable(${TARGET}
    ${SOURCES}
)
target_link_libraries(${TARGET}
    PRIVATE
    sdk
    nnrpc
    fmt::fmt
    nng::nng
    Qt6::Core
    Qt6::Concurrent
)
target_compile_options(${TARGET} PRIVATE "/utf-8" "/MT$<$<CONFIG:Debug>:d>")
target_compile_definitions(${TARGET} PRIVATE "_UNICODE")

# 修改从VS启动时的工作目录路径
set_target_properties(${TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_OUTPUT_DIRECTORY_INTDIR})

# 拷贝配置文件
add_custom_command(
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_OUTPUT_DIRECTORY_INTDIR}/config.json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/config.json
)
