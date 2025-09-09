#include "sf_vulkan/frame.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_vulkan/buffer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace sf {

VkVertexInputBindingDescription Vertex::get_binding_descr() {
    return {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
}

// Positions
#define TLN -0.5f, -0.5f, 0.0f
#define TLF -0.5f, -0.5f, 1.0f
#define TRN +0.5f, -0.5f, 0.0f
#define TRF +0.5f, -0.5f, 1.0f
#define BLN -0.5f, +0.5f, 0.0f
#define BLF -0.5f, +0.5f, 1.0f
#define BRN +0.5f, +0.5f, 0.0f
#define BRF +0.5f, +0.5f, 1.0f

// TODO: Colors

DynamicArray<Vertex> define_vertices() {
    return {
        {{ TLN }, { 1.0f, 0.0f, 0.0f }},
        {{ TRN }, { 0.0f, 1.0f, 0.0f }},
        {{ BRN }, { 0.0f, 0.0f, 1.0f }},
        {{ BLN }, { 0.0f, 0.5f, 0.5f }},
    };
}

DynamicArray<u16> define_indices() {
    return { 0, 1, 2, 2, 3, 0 };
}

void update_ubo(VulkanUniformBufferObject& ubo, void* ubo_mapped_mem, f64 elapsed_time, f32 aspect_ratio) {
    // NOTE: Debug
    LOG_DEBUG("t: {}", elapsed_time);

    ubo.model = glm::rotate(glm::mat4(1.0f), static_cast<f32>(elapsed_time) * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(45.0f, aspect_ratio, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1.0f;
    sf_mem_copy(ubo_mapped_mem, &ubo, sizeof(ubo));
}
    
} // sf

