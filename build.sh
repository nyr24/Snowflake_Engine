#!/bin/sh

DEBUG_BUILD_DIR="build/debug/"
RELEASE_BUILD_DIR="build/release/"
MAKE_OPST=""
ENGINE_MAKE_OPTS=""

BUILD_MODE="debug"
# can be: win32 | wl | x11
PLATFORM="win32"
CLEANUP=0
ENGINE_LIMIT_FPS=0
ENGINE_BUILD_TESTS=0

X11_BUILD_FLAG_SPECIFIED=0;
WAYLAND_BUILD_FLAG_SPECIFIED=0;

for arg in "$@"; do
  case "$arg" in
    -r | --release)
      BUILD_MODE="release"
      ;;
    -c | --clean)
      CLEANUP=1
      ;;
    -wl | --wayland)
      if [ $X11_BUILD_FLAG_SPECIFIED -eq 1 ]; then
        echo "[Error]: can't build for x11 and build simultaneously, exiting"
        exit
      fi
      PLATFORM="wl"
      WAYLAND_BUILD_FLAG_SPECIFIED=1
      ;;
    -x11 | --x11)
      if [ $WAYLAND_BUILD_FLAG_SPECIFIED -eq 1 ]; then
        echo "[Error]: can't build for x11 and build simultaneously, exiting"
        exit
      fi
      PLATFORM="x11"
      X11_BUILD_FLAG_SPECIFIED=1
      ;;
    --limit_fps)
      ENGINE_LIMIT_FPS=1
      ;;
    --test | -t)
      BUILD_TESTS=1
      ;;
    *)
      echo "Unknown argument: $arg"
      ;;
  esac
done

MAKE_OPTS+="BUILD_MODE=$BUILD_MODE "

case $PLATFORM in
  "win32")
    echo "Building for windows"
    MAKE_OPTS+="PLATFORM=win32 "
    ;;
  "wl")
    echo "Building for linux (wayland)"
    MAKE_OPTS+="PLATFORM=wl "
    ;;
  "x11")
    echo "Building for linux (x11)"
    MAKE_OPTS+="PLATFORM=x11 "
    ;;
esac

if [[ $ENGINE_LIMIT_FPS -eq 1 ]] then
  echo "FPS-limit enabled"
  ENGINE_MAKE_OPTS+="LIMIT_FPS=1"
fi

if [[ $BUILD_TESTS -eq 1 ]] then
  echo "Building tests"
  ENGINE_MAKE_OPTS+="BUILD_TESTS=1"
fi

# build engine
cd engine
if [[ $CLEANUP -eq 1 ]] then
  make clean
fi
make $MAKE_OPTS $ENGINE_MAKE_OPTS -j$(nproc)

cd ..

# build testbed
cd testbed
if [[ $CLEANUP -eq 1 ]] then
  make clean
fi
make $MAKE_OPTS -j$(nproc)
cd ..
