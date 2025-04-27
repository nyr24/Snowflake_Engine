cmake_minimum_required(VERSION 3.25)
project(all)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ENGINE_DIR ${ROOT_DIR}/engine)
set(TEST_DIR ${ROOT_DIR}/testbed)

set(CXX_FLAGS -Wall -Werror)
set(CXX_DEBUG_FLAGS -g -O0)
set(ENGINE_LIB_NAME vk-engine)
set(TEST_EXE_NAME testbed)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

# if (CMAKE_BUILD_TYPE ST)

# where are no release flags :))
# set(CMAKE_CXX_FLAGS_DEBUG -g -O0)

add_subdirectory(${ENGINE_DIR})
add_subdirectory(${TEST_DIR})
