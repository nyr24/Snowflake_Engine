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

struct VulkanShaderPipeline;

DynamicArray<Vertex>  define_vertices();
DynamicArray<u16>     define_indices();
// TODO + THINK: move this to renderer
void                  update_ubo(VulkanShaderPipeline& pipeline, u32 curr_frame, f32 aspect_ratio);
void                  update_push_constant_block(VulkanShaderPipeline& pipeline, u32 curr_frame, f64 elapsed_time);

} // sf
