cmake_minimum_required(VERSION 3.9)
project(telegram-desktop)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake;${PROJECT_SOURCE_DIR}/modules/")
include(cotire)
include(CTest)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.10)
    cmake_policy(SET CMP0071 NEW)
endif()

##================================================
## Configure packages
##================================================

if (EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
else()
    find_package(OpenSSL REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Needs OpenAL-SOFT
# Install via `brew install openal-soft` and configure with cmake call from README.md
find_package(OpenAL REQUIRED NO_MODULE)
find_package(ZLIB REQUIRED)
find_package(FFmpeg REQUIRED)
find_package(Opus REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

if (NOT SWSCALE_FOUND)
    message(FATAL_ERROR "FFmpeg swscale is required")
endif()
if (NOT SWRESAMPLE_FOUND)
    message(FATAL_ERROR "FFmpeg swresample is required")
endif()

find_program(CCACHE ccache)
if (CCACHE)
    message(STATUS "Enabling ccache")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
endif()

add_subdirectory(Telegram)
