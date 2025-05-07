cmake_minimum_required(VERSION 3.25)
project(${TEST_EXE_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${TEST_DIR}/src)
set(INCLUDE_DIR ${TEST_DIR}/include)
set(ENGINE_INCLUDE_DIR ${ENGINE_DIR}/include)

file(GLOB_RECURSE TESTBED-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE TESTBED-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)

add_executable(${PROJECT_NAME} ${TESTBED-SRCS} ${TESTBED-HEADERS})

target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
  $<$<CONFIG:Debug>:
    -g
    -Og
  >
  $<$<CONFIG:Release>:
    -O3
  >
)

target_compile_definitions(
  ${PROJECT_NAME}
  PUBLIC
  $<$<CONFIG:Debug>:
    -DSF_DEBUG
    -DSF_ASSERTS_ENABLED
  >
  $<$<CONFIG:Release>:
    -DSF_NO_DEBUG
  >
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ENGINE_LIB_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} ${ENGINE_INCLUDE_DIR})
