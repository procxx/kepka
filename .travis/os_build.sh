#!/bin/bash

set -x

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    docker run --rm -v $PWD:/repo -v ~/.travis:/travis berkus/docker-cpp-ci /bin/sh -c "cd /repo/_build_; conan install .. --build missing; /repo/.travis/build.sh" || exit 1
fi

if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    cd _build_
    export CC=clang
    export CXX=clang++
    export EXTRA_CMAKE_FLAGS=-DCMAKE_PREFIX_PATH='/usr/local/opt/qt5/;/usr/local/opt/openal-soft'
    conan install -s compiler=apple-clang .. --build missing
    ../.travis/build.sh || exit 1
fi
