cmake_minimum_required(VERSION 3.25)
project(${ENGINE_LIB_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(SRC_DIR ${ENGINE_DIR}/src)
set(INCLUDE_DIR ${ENGINE_DIR}/include)
set(STB_INCLUDE_DIR ${ENGINE_DIR}/lib/stb)
set(GLFW_DIR ${ENGINE_DIR}/lib/glfw)
set(ASSIMP_DIR ${ENGINE_DIR}/lib/assimp)

set(SHADER_SRC_DIR ${ENGINE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_COMPILER $ENV{VULKAN_SDK}/bin/slangc)
set(SHADER_TARGET ${PROJECT_NAME}_shaders)

# compile shaders
function (add_shaders target)
  set(SHADER_DIR_TARGET ${SHADER_TARGET}_dir)
  add_custom_target(${SHADER_DIR_TARGET} DEPENDS ${SHADER_BIN_DIR})

  # Create shader binary directory
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    OUTPUT ${SHADER_BIN_DIR}
    COMMENT "Creating shader binary directory"
  )

  # Process each shader file
  foreach(SHADER_PATH ${VK-SHADER-SOURCES})
    get_filename_component(SHADER_FILE_NAME ${SHADER_PATH} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_PATH} EXT)

    if(${EXT} MATCHES "slang")
      set(ENTRY "-entry vertMain -entry fragMain")
    elseif(${EXT} MATCHES "(comp|compute)")
      set(ENTRY "-entry main")
    elseif(${EXT} MATCHES "(geom|geometry)")
      set(ENTRY "-entry geomMain")
    elseif(${EXT} MATCHES "(tesc|tessellation_control)")
      set(ENTRY "-entry tescMain")
    elseif(${EXT} MATCHES "(tese|tessellation_evaluation)")
      set(ENTRY "-entry teseMain")
    endif()

    # Create SPIR-V output path
    set(SPIRV_OUTPUT ${SHADER_BIN_DIR}/${SHADER_FILE_NAME}.spv)

    # Add compilation command
    add_custom_command(
      OUTPUT ${SPIRV_OUTPUT}
      COMMAND ${SHADER_COMPILER}
        ${SHADER_PATH}
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -fvk-use-entrypoint-name
        ${ENTRY}
        -o ${SPIRV_OUTPUT}
      DEPENDS ${SHADER_PATH} ${SHADER_BINARY_DIR}
      COMMENT "Compiling ${SHADER_FILE_NAME}"
    )

    # Collect all SPIR-V outputs
    list(APPEND SPIRV_FILES ${SPIRV_OUTPUT})
  endforeach()

  add_custom_target(${SHADER_TARGET} DEPENDS ${SHADER_DIR_TARGET} ${SPIRV_FILES})

  add_dependencies(${PROJECT_NAME} ${SHADER_TARGET})
endfunction()

file(GLOB_RECURSE VK-ENGINE-SRCS CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE VK-ENGINE-HEADERS CONFIGURE_DEPENDS ${INCLUDE_DIR}/*.hpp)
file(GLOB_RECURSE VK-SHADER-SOURCES CONFIGURE_DEPENDS ${SHADER_SRC_DIR}/*.slang)

# glfw
add_subdirectory(${GLFW_DIR})

# assimp
# https://github.com/svatostop/anthrax-ai/blob/main/engine/libs/CMakeLists.txt
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
# set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE INTERNAL "" FORCE)
configure_file("${ASSIMP_DIR}/include/assimp/config.h.in" "${ASSIMP_DIR}/include/assimp/config.h")
configure_file("${ASSIMP_DIR}/contrib/zlib/zconf.h.cmakein" "${ASSIMP_DIR}/contrib/zlib/zconf.h")
add_subdirectory(${ASSIMP_DIR})

# engine library
add_library(${PROJECT_NAME} SHARED ${VK-ENGINE-SRCS} ${VK-ENGINE-HEADERS})

# shaders
add_shaders(${PROJECT_NAME})

target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
  $<$<CONFIG:Debug>:
    -g
    -O0
  >
  $<$<CONFIG:Release>:
    -O2
  >
)

if (DEFINED SF_SANITIZE)
target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
  -fsanitize=undefined
  -fsanitize=address
)

target_link_options(
  ${PROJECT_NAME}
  PUBLIC
  -fsanitize=undefined
  -fsanitize=address
)
endif()

target_compile_definitions(
  ${PROJECT_NAME}
  PUBLIC
  $<$<CONFIG:Debug>:
    -DSF_DEBUG
    -DSF_ASSERTS_ENABLED
    -DGLM_FORCE_DEPTH_ZERO_TO_ONE
  >
  $<$<CONFIG:Release>:
    -DSF_RELEASE
    -DGLM_FORCE_DEPTH_ZERO_TO_ONE
  >
)

if (DEFINED SF_BUILD_TESTS)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_TESTS)
endif()

if (DEFINED SF_BUILD_LIMIT_FRAME_COUNT)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_LIMIT_FRAME_COUNT)
endif()

if (DEFINED SF_BUILD_WAYLAND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} $ENV{VULKAN_SDK}/include ${GLM_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${ASSIMP_DIR}/include)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_BUILD_WAYLAND)
  target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/lib)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan wayland-client xkbcommon glfw assimp libminizip.so)
elseif (DEFINED SF_BUILD_X11)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} $ENV{VULKAN_SDK}/include ${GLM_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${ASSIMP_DIR}/include)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_BUILD_X11)
  target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/lib)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan xcb xcb-xkb X11 X11-xcb glfw assimp)
else ()
  target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR} ${INCLUDE_DIR} $ENV{VULKAN_SDK}/Include ${GLM_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${ASSIMP_DIR}/include)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SF_BUILD_WINDOWS)
  target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/Lib)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1 glfw assimp)
endif()
