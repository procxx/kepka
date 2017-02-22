cmake_minimum_required(VERSION 3.5)
project(tdesktop)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake;${PROJECT_SOURCE_DIR}/modules/")
include(cotire)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Needs OpenAL-SOFT
# Install via `brew install openal-soft` and configure with `env OPENALDIR=/usr/local/opt/openal-soft`
find_package(OpenAL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibZip REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(GTK3 3.0 REQUIRED gtk)

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
#target_link_libraries(publisher Qt5::Widgets Qt5::DBus)
