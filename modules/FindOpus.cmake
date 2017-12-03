# - Try to find Opus
# Once done this will define
#
#  OPUS_FOUND - system has opus
#  OPUS_INCLUDE_DIR - the opus include directory
#  OPUS_LIBRARIES - Link these to use opus

include(FindPackageHandleStandardArgs)

find_library(OPUS_LIBRARIES opus)
find_path(OPUS_INCLUDE_DIR opus/opus.h)

find_package_handle_standard_args(Opus DEFAULT_MSG OPUS_INCLUDE_DIR OPUS_LIBRARIES)
