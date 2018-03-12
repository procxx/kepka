cmake_minimum_required(VERSION 3.10)
project(telegram-desktop)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake;${PROJECT_SOURCE_DIR}/modules/")
include(cotire)
include(CTest)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.10)
    cmake_policy(SET CMP0071 NEW)
endif()

if (UNIX AND NOT APPLE)
    set(LINUX 1)
endif()

function(set_flag_if_supported target shortname flag)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(${flag} ${shortname}_FLAG_SUPPORTED)
    if (${shortname}_FLAG_SUPPORTED)
        target_compile_options(${target} PRIVATE ${flag})
    endif()
endfunction()

##================================================
## Configure packages
##================================================

if (EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
else()
    find_package(OpenSSL REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (LINUX)
    find_package(OpenAL REQUIRED)
    find_package(ALSA)
    find_package(PulseAudio)
    find_package(Qt5 COMPONENTS DBus)
else(LINUX)
    # Needs OpenAL-SOFT
    # Install via `brew install openal-soft` and configure with cmake call from README.md
    find_package(OpenAL REQUIRED NO_MODULE)
    set(OPENAL_LIBRARY OpenAL::OpenAL)
endif(LINUX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

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
