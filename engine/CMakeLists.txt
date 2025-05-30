cmake_minimum_required(VERSION 3.25)
project(${ENGINE_LIB_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${ENGINE_DIR}/src)
set(INCLUDE_DIR ${ENGINE_DIR}/include)
set(GLM_INCLUDE_DIR ${ENGINE_DIR}/lib/glm)

file(GLOB_RECURSE VK-ENGINE-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE VK-ENGINE-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)

add_library(${PROJECT_NAME} SHARED ${VK-ENGINE-SRCS} ${VK-ENGINE-HEADERS})

add_subdirectory(${GLM_INCLUDE_DIR})

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
    -DWAYLAND_DEBUG
  >
  $<$<CONFIG:Release>:
    -DSF_RELEASE
  >
)

if (DEFINED SF_BUILD_WAYLAND)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_BUILD_WAYLAND)
elseif (DEFINED SF_BUILD_X11)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_BUILD_X11)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} $ENV{VULKAN_SDK}/include ${GLM_INCLUDE_DIR})
target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/lib)
# TODO: link with platform-specific libs under condition
target_link_libraries(${PROJECT_NAME} PRIVATE vulkan glm wayland-client xkbcommon)
