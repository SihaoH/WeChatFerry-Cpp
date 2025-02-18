set(TARGET smc)

find_package(mp3lame REQUIRED)

set(SMC_DIR ${PROJECT_SOURCE_DIR}/SilkMp3Converter/SilkMp3Converter/Codec)

file(GLOB_RECURSE SILK_SDK_C "${SMC_DIR}/SILK_SDK_SRC_FLP_v1.0.9/src/*.c")
file(GLOB_RECURSE SILK_SDK_H "${SMC_DIR}/SILK_SDK_SRC_FLP_v1.0.9/src/*.h")

set(SOURCES
    ${SILK_SDK_C}
    ${SMC_DIR}/codec.cpp
    ${SMC_DIR}/mp3_encoder.cpp
    ${SMC_DIR}/silk_decoder.cpp
)

set(HEADERS
    ${SILK_SDK_H}
    ${SMC_DIR}/codec.h
)

add_library(${TARGET} STATIC ${SOURCES} ${HEADERS})

target_link_libraries(${TARGET}
    PRIVATE
    mp3lame::mp3lame
)

target_include_directories(${TARGET}
    PUBLIC
    ${SMC_DIR}
    PRIVATE
    ${SMC_DIR}/SILK_SDK_SRC_FLP_v1.0.9/src
    ${SMC_DIR}/SILK_SDK_SRC_FLP_v1.0.9/interface
)

target_compile_options(${TARGET} PRIVATE "/permissive-" "/MT$<$<CONFIG:Debug>:d>")
