#!/bin/bash

set -x

cmake -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE $EXTRA_CMAKE_FLAGS -DBUILD_TESTING=ON .. || exit 1
# grep returns number of items found. each change is enclosed into <replacement>
# tag in the xml. Thus if no changes needed, 0 will be returned
cmake --build . --target clang-format -- -v
if [[ $(git diff) ]]; then
    git diff | cat
    exit 1
else
    echo "Success!"
fi
cmake --build . -- -v || exit 1
ASAN_OPTIONS=alloc_dealloc_mismatch=0 ctest . || exit 1
