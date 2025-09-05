#pragma once

#include "sf_containers/dynamic_array.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription get_binding_descr();
};

DynamicArray<Vertex>  define_vertices();
DynamicArray<u16>     define_indices();

} // sf
