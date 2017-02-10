cmake_minimum_required(VERSION 3.7)
project(tdesktop)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cotire/CMake")
include(cotire)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(OpenAL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

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

# OPENAL_LIBRARY
# ZLIB_LIBRARIES
# OPENSSL_LIBRARIES

include_directories(${OPENAL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

add_subdirectory(Telegram)
#target_link_libraries(publisher Qt5::Widgets Qt5::DBus)
