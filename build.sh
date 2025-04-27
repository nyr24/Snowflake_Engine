#!/bin/bash

DEBUG_BUILD_DIR="build/debug/"
RELEASE_BUILD_DIR="build/release/"
BUILD_RELEASE=0
CMAKE_OPTS="-DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_CXX_COMPILER=clang++"
CMAKE_BUILD_OPTS="-j"

for arg in "$@"; do
  case "$arg" in
  -r | --release)
    BUILD_RELEASE=1
    CMAKE_OPTS="-DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_CXX_COMPILER=clang++"
    ;;
  -c | --clean)
    echo "Performing cleanup..."
    CMAKE_BUILD_OPTS+=" --clean-first"
    ;;
  *)
    echo "Unknown argument: $arg"
    ;;
  esac
done

if [ $BUILD_RELEASE -eq 0 ]; then
  echo "Building in debug mode"
  [ -d "$DEBUG_BUILD_DIR" ] || mkdir "$DEBUG_BUILD_DIR"
  cd "$DEBUG_BUILD_DIR"
  cmake $CMAKE_OPTS ../../ && cmake --build . $CMAKE_BUILD_OPTS
else
  echo "Building in release mode"
  [ -d "$RELEASE_BUILD_DIR" ] || mkdir "$RELEASE_BUILD_DIR"
  cd "$RELEASE_BUILD_DIR"
  cmake $CMAKE_OPTS ../../ && cmake --build . $CMAKE_BUILD_OPTS
fi
