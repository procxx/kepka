cmake_minimum_required(VERSION 3.10)
project(telegram-desktop)

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
find_package(OpenSSL REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(GTK3 3.0 REQUIRED gtk)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

#find_package(PortAudio REQUIRED)
#find_package(Opus REQUIRED)
#find_package(FFmpeg REQUIRED)

if (NOT OPENAL_FOUND)
    message(FATAL_ERROR OpenAL Soft is required)
endif()
if (NOT ZLIB_FOUND)
    message(FATAL_ERROR Zlib is required)
endif()
if (NOT OPENSSL_FOUND)
    message(FATAL_ERROR OpenSSL is required)
endif()

add_subdirectory(Telegram)
