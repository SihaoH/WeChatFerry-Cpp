set(TARGET app)

find_package(nng REQUIRED)

add_executable(${TARGET}
    main.cpp
)
target_link_libraries(${TARGET}
    PRIVATE
    sdk
    nnrpc
    nng::nng
)
target_compile_definitions(${TARGET} PRIVATE "_UNICODE")

# 修改从VS启动时的工作目录路径
set_target_properties(${TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_OUTPUT_DIRECTORY_INTDIR})
