cmake_minimum_required(VERSION 3.25)
project(${ENGINE_LIB_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${ENGINE_DIR}/src)
set(INCLUDE_DIR ${ENGINE_DIR}/include)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE VK-ENGINE-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE VK-ENGINE-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)

add_library(${PROJECT_NAME} ${VK-ENGINE-SRCS} ${VK-ENGINE-HEADERS})

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
  $<$<CONFIG:Debug>:
    -g
    -Og
    -DSN_DEBUG
  >
  $<$<CONFIG:Release>:
    -O3
    -DSN_NO_DEBUG
  >
)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR} ${Vulkan_INCLUDE_DIRS})
