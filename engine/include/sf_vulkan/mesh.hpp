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

struct Mesh {
    DynamicArray<Vertex>  vertices;
    DynamicArray<u16>     indices;

    static Mesh get_cube_mesh();
private:
    static DynamicArray<Vertex> define_cube_vertices();
    static DynamicArray<u16> define_cube_indices();
};

} // sf
