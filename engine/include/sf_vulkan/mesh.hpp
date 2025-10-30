#pragma once

#include "sf_allocators/linear_allocator.hpp"
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
    DynamicArray<Vertex, LinearAllocator>  vertices;
    DynamicArray<u16, LinearAllocator>     indices;

    static Mesh get_cube_mesh(LinearAllocator& allocator);
private:
    static DynamicArray<Vertex, LinearAllocator> define_cube_vertices(LinearAllocator& allocator);
    static DynamicArray<u16, LinearAllocator> define_cube_indices(LinearAllocator& allocator);
};

} // sf
