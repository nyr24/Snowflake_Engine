cmake_minimum_required(VERSION 3.25)
project(${TEST_EXE_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${TEST_DIR}/src)
set(INCLUDE_DIR ${TEST_DIR}/include)
set(ENGINE_INCLUDE_DIR ${ENGINE_DIR}/include)

include_directories(${INCLUDE_DIR} ${ENGINE_INCLUDE_DIR})

file(GLOB_RECURSE TESTBED-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE TESTBED-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS ${ENGINE_INCLUDE_DIR}/*.hpp)

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

add_executable(${PROJECT_NAME} ${TESTBED-SRCS} ${TESTBED-HEADERS} ${ENGINE_HEADERS})

target_link_libraries(${PROJECT_NAME} PUBLIC ${ENGINE_LIB_NAME})
