#!/bin/bash

DEBUG_BUILD_DIR="build/debug/"
RELEASE_BUILD_DIR="build/release/"
BUILD_RELEASE=0

if [ $# -gt 0 ] && [ "$1" = "-r" ] || [ "$1" = "--release" ]; then
  BUILD_RELEASE=1
fi

if [ $BUILD_RELEASE == 0 ]; then
  echo "Building in debug mode"
  [ -d "$DEBUG_BUILD_DIR" ] || mkdir "$DEBUG_BUILD_DIR"
  cd "$DEBUG_BUILD_DIR"
  cmake -DCMAKE_BUILD_TYPE=DEBUG ../../ && cmake --build . -j
else
  echo "Building in release mode"
  [ -d "$RELEASE_BUILD_DIR" ] || mkdir "$RELEASE_BUILD_DIR"
  cd "$DEBUG_BUILD_DIR"
  cmake -DCMAKE_BUILD_TYPE=RELEASE ../../ && cmake --build . -j
fi
