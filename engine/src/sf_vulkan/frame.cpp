#include "sf_vulkan/frame.hpp"
#include "sf_containers/dynamic_array.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

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
        {{ TLN }, { +0.0f, +1.0f }},
        {{ TRN }, { +1.0f, +1.0f }},
        {{ BRN }, { +1.0f, +0.0f }},
        {{ BLN }, { +0.0f, +0.0f }},
    };
}

DynamicArray<u16> define_indices() {
    return { 0, 1, 2, 2, 3, 0 };
}

    
} // sf

