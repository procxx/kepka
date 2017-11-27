cmake_minimum_required(VERSION 3.10)
project(telegram-desktop)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
else()
    find_package(OpenSSL REQUIRED)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake;${PROJECT_SOURCE_DIR}/modules/")
include(cotire)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Ignore automoc-ing generated files (@tbd?)
cmake_policy(SET CMP0071 OLD)

# Needs OpenAL-SOFT
# Install via `brew install openal-soft` and configure with `env OPENALDIR=/usr/local/opt/openal-soft`
find_package(OpenAL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibZip REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(FFmpeg REQUIRED)
#@todo Turn into find_package(Opus REQUIRED)
find_library(OPUS_LIB opus)
find_path(OPUS_INCLUDE_DIR opus/opus.h)

if (NOT SWSCALE_FOUND)
    message(FATAL_ERROR "FFmpeg swscale is required")
endif()
if (NOT SWRESAMPLE_FOUND)
    message(FATAL_ERROR "FFmpeg swresample is required")
endif()
if (NOT OPUS_LIB)
    message(FATAL_ERROR "opus codec is required")
endif()

#find_package(PortAudio REQUIRED)

add_subdirectory(Telegram)
