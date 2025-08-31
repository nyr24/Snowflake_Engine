cmake_minimum_required(VERSION 3.25)
project(all)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ENGINE_DIR ${ROOT_DIR}/engine)
set(TEST_DIR ${ROOT_DIR}/testbed)
set(ENGINE_LIB_NAME snowflake-engine)
set(TEST_EXE_NAME snowflake-test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${ENGINE_DIR})
add_subdirectory(${TEST_DIR})
