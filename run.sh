EXE_NAME="testbed"
DEBUG_EXE_PATH="./build/debug/${EXE_NAME}"
RELEASE_EXE_PATH="./build/release/${EXE_NAME}"
RUN_RELEASE=0

if [ "$1" = "-r" ] || [ "$1" = "--release" ]; then
  RUN_RELEASE=1
fi

if [ $RUN_RELEASE -eq 0 ]; then
  $DEBUG_EXE_PATH
else
  $RELEASE_EXE_PATH
fi
