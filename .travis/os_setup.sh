#!/bin/bash

set -x

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    docker pull procpp/docker-cpp-ci:latest || exit 1
fi

brew_package() {
    if brew ls --versions $1 > /dev/null; then
        brew outdated $1 > /dev/null || brew upgrade $1 || exit 1
    else
        brew install $1 || exit 1
    fi
}

if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    brew update
    brew_package conan
    brew_package cmake
    brew_package ninja
    brew_package qt
    brew_package ffmpeg
    brew_package opus
    brew_package openal-soft
fi
