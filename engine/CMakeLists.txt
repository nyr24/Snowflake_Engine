cmake_minimum_required(VERSION 3.25)
project(${ENGINE_LIB_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${ENGINE_DIR}/src)
set(INCLUDE_DIR ${ENGINE_DIR}/include)

file(GLOB_RECURSE VK-ENGINE-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE VK-ENGINE-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)

add_library(${PROJECT_NAME} ${VK-ENGINE-SRCS} ${VK-ENGINE-HEADERS})

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
  $<$<CONFIG:Debug>:
    -g
    -Og
    -DSF_DEBUG
    -DSF_ASSERTS_ENABLED
  >
  $<$<CONFIG:Release>:
    -O3
    -DSF_NO_DEBUG
  >
)
target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} $ENV{VULKAN_SDK}/include)
target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/lib)
target_link_libraries(${PROJECT_NAME} PRIVATE vulkan)
