#pragma once

#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texture_coord;

    static VkVertexInputBindingDescription get_binding_descr();
};

struct VulkanShaderPipeline;

struct Mesh {
    DynamicArrayBacked<Vertex, StackAllocator>  vertices;
    DynamicArrayBacked<u16, StackAllocator>     indices;

    static Mesh get_cube_mesh(StackAllocator& allocator);
private:
    static DynamicArrayBacked<Vertex, StackAllocator> define_cube_vertices(StackAllocator& allocator);
    static DynamicArrayBacked<u16, StackAllocator> define_cube_indices(StackAllocator& allocator);
};

} // sf
