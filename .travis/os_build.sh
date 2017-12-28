#!/bin/bash

set -x

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    docker run --rm -v $PWD:/repo -v ~/.travis:/travis berkus/docker-cpp-ci /bin/sh -c "cd /repo/_build_; /repo/.travis/build.sh"
fi

if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    cd _build_
    ../.travis/build.sh
fi
