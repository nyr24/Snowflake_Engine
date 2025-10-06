#include "sf_vulkan/mesh.hpp"
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
    }; }

// Positions
#define POS_TLN -0.5f, -0.5f, 0.0f
#define POS_TLF -0.5f, -0.5f, 1.0f
#define POS_TRN +0.5f, -0.5f, 0.0f
#define POS_TRF +0.5f, -0.5f, 1.0f
#define POS_BLN -0.5f, +0.5f, 0.0f
#define POS_BLF -0.5f, +0.5f, 1.0f
#define POS_BRN +0.5f, +0.5f, 0.0f
#define POS_BRF +0.5f, +0.5f, 1.0f

// Texture positions
#define TEX_TL  +0.0f, +1.0f
#define TEX_TR  +1.0f, +1.0f
#define TEX_BL  +0.0f, +0.0f
#define TEX_BR  +1.0f, +0.0f

// Normals
#define NORMAL_TOP      +0.0f, +1.0f, +0.0f
#define NORMAL_BOTTOM   +0.0f, -1.0f, +0.0f
#define NORMAL_RIGHT    +1.0f, +0.0f, +0.0f
#define NORMAL_LEFT     -1.0f, +0.0f, +0.0f
#define NORMAL_FORWARD  +0.0f, +0.0f, +1.0f
#define NORMAL_BACKWARD +0.0f, +0.0f, -1.0f

// Colors
#define WHITE   1.0f, 1.0f, 1.0f
#define BLACK   0.0f, 0.0f, 0.0f
#define RED     1.0f, 0.0f, 0.0f
#define GREEN   0.0f, 1.0f, 0.0f
#define BLUE    0.0f, 0.0f, 1.0f
#define ORANGE  1.0f, 0.5f, 0.1f
#define PURPLE  0.5f, 0.0f, 0.5f
#define YELLOW  1.0f, 1.0f, 0.0f

DynamicArray<Vertex> Mesh::define_cube_vertices() {
    return {
        // forward
        {{ POS_TLN }, { TEX_TL }},
        {{ POS_BLN }, { TEX_BL }},
        {{ POS_BRN }, { TEX_BR }},
        {{ POS_TRN }, { TEX_TR }},

        // backward
        {{ POS_TLF }, { TEX_TL }},
        {{ POS_BLF }, { TEX_BL }},
        {{ POS_BRF }, { TEX_BR }},
        {{ POS_TRF }, { TEX_TR }},

        // up
        {{ POS_TLN }, { TEX_TL }},
        {{ POS_TRN }, { TEX_BL }},
        {{ POS_TLF }, { TEX_BR }},
        {{ POS_TRF }, { TEX_TR }},

        // down
        {{ POS_BLN }, { TEX_TL }},
        {{ POS_BRN }, { TEX_BL }},
        {{ POS_BLF }, { TEX_BR }},
        {{ POS_BRF }, { TEX_TR }},

        // right
        {{ POS_TRN }, { TEX_TL }},
        {{ POS_BRN }, { TEX_BL }},
        {{ POS_TRF }, { TEX_BR }},
        {{ POS_BRF }, { TEX_TR }},

        // left
        {{ POS_TLN }, { TEX_TL }},
        {{ POS_BLN }, { TEX_BL }},
        {{ POS_TLF }, { TEX_BR }},
        {{ POS_BLF }, { TEX_TR }},
    };
}

// TODO: mesh class with static method cube_mesh()
DynamicArray<u16> Mesh::define_cube_indices() {
    return {
        0, 1, 2,        0, 2, 3,
        4, 6, 5,        4, 7, 6,
        8, 9, 10,       9, 11, 10,
        12, 14, 13,     13, 14, 15,
        16, 17, 18,     17, 19, 18,
        20, 22, 21,     21, 22, 23
    };
}

Mesh Mesh::get_cube_mesh() {
    return {
        .vertices = define_cube_vertices(),
        .indices = define_cube_indices()
    };
}
    
} // sf

