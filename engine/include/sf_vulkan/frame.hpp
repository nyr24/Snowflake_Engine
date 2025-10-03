#pragma once

#include "sf_containers/dynamic_array.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

struct Vertex {
    glm::vec3 pos;
    // glm::vec3 color;
    glm::vec2 texture_coord;

    static VkVertexInputBindingDescription get_binding_descr();
};

struct VulkanShaderPipeline;

DynamicArray<Vertex>  define_vertices();
DynamicArray<u16>     define_indices();

} // sf
