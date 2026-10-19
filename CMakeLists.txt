cmake_minimum_required(VERSION 3.15)
project(Playson VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_HAS_STD_BYTE=0)

include_directories(
    include
    src
)

set(SOURCES
    src/plugin.cpp
    src/ui_win32.cpp
    src/wav_encoder.h
    src/http_client.h
    src/ui_win32.h
)

add_library(Playson SHARED ${SOURCES})

# In CLAP specification on Windows, plugins are shared libraries (.clap)
set_target_properties(Playson PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    OUTPUT_NAME "Playson"
)

target_link_libraries(Playson PRIVATE
    winhttp
    gdiplus
    gdi32
    user32
    ole32
)
