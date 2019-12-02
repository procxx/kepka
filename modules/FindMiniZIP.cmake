# - Try to find MiniZIP
# Once done this will define
#
#  MINIZIP_FOUND - system has minizip
#  MINIZIP_INCLUDE_DIR - the minizip include directory
#  MINIZIP_LIBRARIES - Link these to use minizip

find_package(PkgConfig)
pkg_check_modules(PC_MINIZIP QUIET minizip)

include(FindPackageHandleStandardArgs)

find_library(MINIZIP_LIBRARIES minizip)
find_path(MINIZIP_INCLUDE_DIR zip.h ${PC_MINIZIP_INCLUDE_DIRS})

find_package_handle_standard_args(MiniZIP DEFAULT_MSG MINIZIP_INCLUDE_DIR MINIZIP_LIBRARIES)
