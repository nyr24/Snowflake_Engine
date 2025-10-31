cmake_minimum_required(VERSION 3.25)
set(CMAKE_C_COMPILER_WORKS "true")
set(CMAKE_CXX_COMPILER_WORKS "true")
set(CMAKE_LINKER "/usr/bin/clang++")
project(all)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ENGINE_DIR ${ROOT_DIR}/engine)
set(TEST_DIR ${ROOT_DIR}/testbed)
set(ENGINE_LIB_NAME snowflake-engine)
set(GLM_INCLUDE_DIR ${ENGINE_DIR}/lib/glm)
set(TEST_EXE_NAME snowflake-test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${ENGINE_DIR})
add_subdirectory(${TEST_DIR})
add_subdirectory(${GLM_INCLUDE_DIR})
