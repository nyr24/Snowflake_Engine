#!/bin/sh

DEBUG_BUILD_DIR="build/debug/"
RELEASE_BUILD_DIR="build/release/"

BUILD_RELEASE=0
X11_BUILD_FLAG_SPECIFIED=0;
WAYLAND_BUILD_FLAG_SPECIFIED=0;

CMAKE_OPTS=""
CMAKE_BUILD_OPTS="-j"
BUILD_TYPE="debug"
# win32 | wl | x11
PLATFORM="win32"
COMPILER="clang++"

for arg in "$@"; do
  case "$arg" in
  -r | --release)
    BUILD_TYPE="release"
    ;;
  -c | --clean)
    echo "Performing cleanup..."
    CMAKE_BUILD_OPTS+=" --clean-first"
    ;;
  -wl | --wayland)
    if [ $X11_BUILD_FLAG_SPECIFIED -eq 1 ]; then
      echo "[Error]: can't build for x11 and wayland build simultaneously, exiting"
      exit
    fi
    PLATFORM="wl"
    WAYLAND_BUILD_FLAG_SPECIFIED=1
    ;;
  -x11 | --x11)
    if [ $WAYLAND_BUILD_FLAG_SPECIFIED -eq 1 ]; then
      echo "[Error]: can't build for x11 and wayland build simultaneously, exiting"
      exit
    fi
    PLATFORM="x11"
    X11_BUILD_FLAG_SPECIFIED=1
    ;;
  -win32 | --windows)
    echo "Windows is bad, consider switching to Linux"
    PLATFORM="win32"
    ;;
  --limit_fps)
    echo "FPS limit option enabled"
    CMAKE_OPTS+=" -DSF_BUILD_LIMIT_FRAME_COUNT=1"
    X11_BUILD_FLAG_SPECIFIED=1
    ;;
  --test | -t)
    echo "Building tests..."
    CMAKE_OPTS+=" -DSF_BUILD_TESTS=1"
    ;;
  --gcc)
    echo "Using gcc compiler"
    COMPILER="g++"
    ;;
  *)
    echo "Unknown argument: $arg"
    ;;
  esac
done

if [ $PLATFORM == "win32" ]; then
    CMAKE_OPTS+=" -DSF_BUILD_WINDOWS=1"
    echo "Building for windows..."
elif [ $PLATFORM == "wl" ]; then
    CMAKE_OPTS+=" -DSF_BUILD_WAYLAND=1"
    WAYLAND_BUILD_FLAG_SPECIFIED=1
    echo "Building for linux (wayland)..."
else
    CMAKE_OPTS+=" -DSF_BUILD_X11=1"
    X11_BUILD_FLAG_SPECIFIED=1
    echo "Building for linux (x11)..."
fi

if [ $WAYLAND_BUILD_FLAG_SPECIFIED -eq 1 && $X11_BUILD_FLAG_SPECIFIED -eq 1 ]; then
  echo "[Error]: Can't build for wayland and x11 simultaneously on linux"
fi

CMAKE_OPTS+=" -DCMAKE_CXX_COMPILER=$COMPILER"
CMAKE_OPTS+=" -DCMAKE_BUILD_TYPE=$BUILD_TYPE"

[ -d "build" ] || mkdir "build"

if [ $BUILD_TYPE == "debug" ]; then
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
