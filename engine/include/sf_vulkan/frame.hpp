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

struct VulkanUniformBufferObject;

DynamicArray<Vertex>  define_vertices();
DynamicArray<u16>     define_indices();
void                  update_ubo(VulkanUniformBufferObject& ubo, void* ubo_mapped_mem, f64 elapsed_time, f32 aspect_ratio);

} // sf
