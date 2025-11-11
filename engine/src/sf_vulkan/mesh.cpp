#include "sf_vulkan/mesh.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/constants.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <initializer_list>
#include <tuple>
#include <vulkan/vulkan_core.h>

namespace sf {

static MeshSystemState* state_ptr{nullptr};

void MeshSystemState::create(ArenaAllocator& main_allocator, StackAllocator& temp_allocator, MeshSystemState &out_state) {
    state_ptr = &out_state;
    out_state.meshes.set_allocator(&main_allocator);
    out_state.meshes.resize(MESH_INIT_CAPACITY);
    for (u32 i{0}; i < out_state.meshes.count(); ++i) {
        auto& mesh = out_state.meshes[i];
        mesh.data.set_allocator(&main_allocator);
        mesh.data.reserve(Mesh::get_memory_requirement());
        mesh.id = i;
        mesh.generation = INVALID_ID;
        mesh.auto_release = false;
        mesh.ref_count = 0;
    }
    out_state.default_mesh.data.set_allocator(&main_allocator);
    out_state.default_mesh.data.reserve(Mesh::get_memory_requirement());
    out_state.create_default_mesh(temp_allocator);
}

void MeshSystemState::create_default_mesh(StackAllocator& temp_allocator) {
    default_mesh.id = INVALID_ID;
    default_mesh.generation = INVALID_ID;
    default_mesh.auto_release = false;
    default_mesh.ref_count = 0;
    auto vertices = define_cube_vertices(temp_allocator);
    auto indices = define_cube_indices(temp_allocator);
    default_mesh.init_from_vertices_and_indices(std::move(vertices), std::move(indices));
}

Mesh* mesh_system_get_mesh(u32 id) {
    auto& mesh{ state_ptr->meshes[id] };
    mesh.ref_count++;
    if (mesh.generation == INVALID_ID) {
        mesh.generation = 0;
    } else {
        mesh.generation++;
    }
    return &state_ptr->meshes[id];
}

Mesh* mesh_system_get_default_mesh() { 
    auto& default_mesh{ state_ptr->default_mesh };
    default_mesh.ref_count++;
    if (default_mesh.generation == INVALID_ID) {
        default_mesh.generation = 0;
    } else {
        default_mesh.generation++;
    }
    return &state_ptr->default_mesh;
}

void mesh_system_free_mesh(u32 id) {
    auto& mesh{ state_ptr->meshes[id] };

    if (mesh.ref_count > 0) {
        mesh.ref_count--;
        if (mesh.ref_count < 1 && mesh.auto_release) {
            mesh.destroy();
        }
    }
}

void Mesh::init_from_vertices_and_indices(DynamicArrayBacked<Vertex, StackAllocator>&& vertices, DynamicArrayBacked<u16, StackAllocator>&& indices) {
    data.resize(vertices.size_in_bytes() + indices.size_in_bytes());
    indeces_count = indices.count();
    indeces_offset = vertices.size_in_bytes(); 
    sf_mem_copy(data.data(), vertices.data(), vertices.size_in_bytes());
    sf_mem_copy(data.data() + indeces_offset, indices.data(), indices.size_in_bytes());
}

void Mesh::destroy() {
    data.clear();
    id = INVALID_ID;
    generation = INVALID_ID;
    ref_count = 0;
}

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
#define NORMAL_UP       +0.0f, +1.0f, +0.0f
#define NORMAL_DOWN     +0.0f, -1.0f, +0.0f
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

DynamicArrayBacked<Vertex, StackAllocator> MeshSystemState::define_cube_vertices(StackAllocator& allocator) {
    return std::tuple<StackAllocator*, std::initializer_list<Vertex>>{
        &allocator,
        {
            // forward
            {{ POS_TLN }, { NORMAL_FORWARD }, { TEX_TL }},
            {{ POS_BLN }, { NORMAL_FORWARD }, { TEX_BL }},
            {{ POS_BRN }, { NORMAL_FORWARD }, { TEX_BR }},
            {{ POS_TRN }, { NORMAL_FORWARD }, { TEX_TR }},

            // backward
            {{ POS_TLF }, { NORMAL_BACKWARD }, { TEX_TL }},
            {{ POS_BLF }, { NORMAL_BACKWARD }, { TEX_BL }},
            {{ POS_BRF }, { NORMAL_BACKWARD }, { TEX_BR }},
            {{ POS_TRF }, { NORMAL_BACKWARD }, { TEX_TR }},

            // up
            {{ POS_TLN }, { NORMAL_UP }, { TEX_TL }},
            {{ POS_TRN }, { NORMAL_UP }, { TEX_TR }},
            {{ POS_TLF }, { NORMAL_UP }, { TEX_BL }},
            {{ POS_TRF }, { NORMAL_UP }, { TEX_BR }},

            // down
            {{ POS_BLN }, { NORMAL_DOWN }, { TEX_TL }},
            {{ POS_BRN }, { NORMAL_DOWN }, { TEX_TR }},
            {{ POS_BLF }, { NORMAL_DOWN }, { TEX_BL }},
            {{ POS_BRF }, { NORMAL_DOWN }, { TEX_BR }},

            // right
            {{ POS_TRN }, { NORMAL_RIGHT }, { TEX_TL }},
            {{ POS_BRN }, { NORMAL_RIGHT }, { TEX_BL }},
            {{ POS_TRF }, { NORMAL_RIGHT }, { TEX_TR }},
            {{ POS_BRF }, { NORMAL_RIGHT }, { TEX_BR }},

            // left
            {{ POS_TLN }, { NORMAL_LEFT }, { TEX_TL }},
            {{ POS_BLN }, { NORMAL_LEFT }, { TEX_BL }},
            {{ POS_TLF }, { NORMAL_LEFT }, { TEX_TR }},
            {{ POS_BLF }, { NORMAL_LEFT }, { TEX_BR }},
        }
    };
}

DynamicArrayBacked<u16, StackAllocator> MeshSystemState::define_cube_indices(StackAllocator& allocator) {
    return std::tuple<StackAllocator*, std::initializer_list<u16>>{
        &allocator,
        {
            0, 1, 2,        0, 2, 3,
            4, 6, 5,        4, 7, 6,
            8, 9, 10,       9, 11, 10,
            12, 14, 13,     13, 14, 15,
            16, 17, 18,     17, 19, 18,
            20, 22, 21,     21, 22, 23
        }
    };
}

VkVertexInputBindingDescription Vertex::get_binding_descr() {
    return {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
}

} // sf

