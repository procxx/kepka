cmake_minimum_required(VERSION 3.10)
project(Kepka
    LANGUAGES C CXX
    VERSION 2.0.0
    DESCRIPTION "Unofficial Telegram Desktop fork"
#    HOMEPAGE_URL "https://github.com/procxx/kepka"
)

set(PROJECT_VERSION_RC 1)

if(PROJECT_VERSION_RC)
    set(PROJECT_VERSION "${PROJECT_VERSION}-rc${PROJECT_VERSION_RC}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/modules/")

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.10)
    cmake_policy(SET CMP0071 NEW)
endif()

# includes block
include(CTest)
# clang-format
include(modules/clang-cxx-dev-tools.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

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

if (LINUX)
    find_package(OpenAL REQUIRED)
    find_package(ALSA)
    find_package(PulseAudio)
    find_package(Qt5 COMPONENTS DBus)
else()
    # Needs OpenAL-SOFT
    # Install via `brew install openal-soft` and configure with cmake call from README.md
    find_package(OpenAL REQUIRED NO_MODULE)
    set(OPENAL_LIBRARY OpenAL::OpenAL)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
find_package(ZLIB REQUIRED)
find_package(FFmpeg REQUIRED)
find_package(Opus REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12.0")
    find_package(Python3 REQUIRED)
else()
    # TODO: remove when we will update cmake required version
    find_package(PythonInterp)
    if((PYTHONINTERP_FOUND) AND (PYTHON_VERSION_MAJOR GREATER 2))
        set(Python3_EXECUTABLE ${PYTHON_EXECUTABLE})
    else()
        message(FATAL_ERROR "Python 3 is required")
    endif()
endif()

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


option(BUILD_DOC "Build documentation" OFF)
mark_as_advanced(BUILD_DOC)

if(BUILD_DOC)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_custom_target( doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
    endif()
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O1 -fno-omit-frame-pointer -g")
add_subdirectory(Telegram)
