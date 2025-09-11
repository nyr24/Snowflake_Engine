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

struct VulkanGlobalUniformObject;
struct VulkanPushConstantBlock;

DynamicArray<Vertex>  define_vertices();
DynamicArray<u16>     define_indices();
void                  update_ubo(VulkanGlobalUniformObject& ubo, void* ubo_mapped_mem, f32 aspect_ratio);
void                  update_push_constant_block(VulkanPushConstantBlock& push_constant_block, f64 elapsed_time);

} // sf
