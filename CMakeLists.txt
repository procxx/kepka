cmake_minimum_required(VERSION 3.10)
project(telegram-desktop)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake;${PROJECT_SOURCE_DIR}/modules/")
include(cotire)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Ignore automoc-ing generated files (@tbd?)
cmake_policy(SET CMP0071 OLD)

find_package(ZLIB REQUIRED)
find_package(LibZip REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(GTK3 3.0 REQUIRED gtk)
find_package(FFmpeg REQUIRED)
find_package(Iconv REQUIRED)
#@todo Turn into find_package(Opus REQUIRED)
find_library(OPUS_LIB opus)
find_path(OPUS_INCLUDE_DIR opus.h PATHS /usr/local/include/opus)

# include_directories(${GTK3_INCLUDE_DIRS})
# link_directories(${GTK3_LIBRARY_DIRS})

if (NOT FFMPEG_SWSCALE_FOUND)
    message(FATAL_ERROR "FFmpeg swscale is required")
endif()
if (NOT FFMPEG_SWRESAMPLE_FOUND)
    message(FATAL_ERROR "FFmpeg swresample is required")
endif()
if (NOT OPUS_LIB)
    message(FATAL_ERROR "opus codec is required")
endif()

#find_package(PortAudio REQUIRED)

add_subdirectory(Telegram)
