cmake_minimum_required(VERSION 3.25)
project(${ENGINE_LIB_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${ENGINE_DIR}/src)
set(INCLUDE_DIR ${ENGINE_DIR}/include)

find_package(Vulkan REQUIRED)

include_directories(
  ${INCLUDE_DIR}
  ${Vulkan_INCLUDE_DIRS}
)

file(GLOB_RECURSE VK-ENGINE-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE VK-ENGINE-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)

# # Common options for all configurations
# add_compile_options(
#   $<$<CONFIG:Debug>:   # Debug configuration
#   -g               # Debug symbols
#   -O0              # No optimization
#   -DDEBUG          # Define DEBUG macro
#   >
#   $<$<CONFIG:Release>: # Release configuration
#   -O3             # Maximum optimization
#   -DNDEBUG        # Define NDEBUG macro
#   >
# )

add_library(${PROJECT_NAME} ${VK-ENGINE-SRCS} ${VK-ENGINE-HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
