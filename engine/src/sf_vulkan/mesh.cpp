#include "sf_vulkan/mesh.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/material.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

// TODO: adding geometries to render queue/array

namespace sf {

static GeometrySystem* state_ptr{nullptr};

void Geometry::create_empty(
    u32 id,
    ArenaAllocator& alloc,
    Geometry& out_geometry
) {
    sf_mem_zero(&out_geometry, sizeof(Geometry));
    out_geometry.id = id;
    out_geometry.generation = INVALID_ID;
    out_geometry.data.set_allocator(&alloc);
    out_geometry.data.reserve(Geometry::get_memory_requirement());
}

void Geometry::create_from_vertices_and_indices(
    u32 id,
    ArenaAllocator& alloc,
    DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
    DynamicArrayBacked<u32, StackAllocator>&& indices,
    Geometry& out_geometry
) {
    Geometry::create_empty(id, alloc, out_geometry);
    out_geometry.set_vertices_and_indices(std::move(vertices), std::move(indices));
}
    
void Geometry::set_vertices_and_indices(
    DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
    DynamicArrayBacked<u32, StackAllocator>&& indices
) {
    data.resize(vertices.size_in_bytes() + indices.size_in_bytes());
    indeces_count = indices.count();
    indeces_offset = vertices.size_in_bytes(); 
    sf_mem_copy(data.data(), vertices.data(), vertices.size_in_bytes());
    sf_mem_copy(data.data_offset(indeces_offset), indices.data(), indices.size_in_bytes());
}

void Geometry::destroy() {
    data.free();    
    id = INVALID_ID;
    indeces_count = 0;
    indeces_offset = 0;
}

// GeometrySystem

void GeometrySystem::create(ArenaAllocator& main_allocator, StackAllocator& temp_allocator, GeometrySystem &out_state) {
    state_ptr = &out_state;
    out_state.geometries.set_allocator(&main_allocator);
    out_state.geometries.resize(INIT_CAPACITY);
    out_state.alloc = &main_allocator;
    
    for (u32 i{0}; i < out_state.geometries.count(); ++i) {
        auto& geometry = out_state.geometries[i];
        Geometry::create_empty(i, main_allocator, geometry);
    }

    Geometry::create_empty(-1, main_allocator, out_state.default_geometry);
    out_state.create_default_geometry(temp_allocator);
}

void GeometrySystem::create_default_geometry(StackAllocator& temp_allocator) {
    auto vertices = define_cube_vertices(temp_allocator);
    auto indices = define_cube_indices(temp_allocator);
    default_geometry.set_vertices_and_indices(std::move(vertices), std::move(indices));
}

Geometry& GeometrySystem::get_empty_slot() {
    SF_ASSERT_MSG(state_ptr && state_ptr->alloc, "Should be valid ptrs");

    auto& geometries = state_ptr->geometries;
    auto* alloc = state_ptr->alloc;
    u32 count{ geometries.count() };
    
    for (u32 i{0}; i < count; ++i) {
        if (geometries[i].generation == INVALID_ID) {
            geometries[i].generation = 0;
            return geometries[i];
        }
    }
    // empty slot not found, do resizing
    u32 free_slot = geometries.count();
    geometries.resize_exponent(free_slot + 1);

    Geometry::create_empty(free_slot, *alloc, geometries[free_slot]);
    count = geometries.count();

    for (u32 i{free_slot + 1}; i < count; ++i) {
        Geometry::create_empty(i, *alloc, geometries[i]);
    }

    return geometries[free_slot];
}

Geometry& GeometrySystem::create_geometry_and_get(
    DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
    DynamicArrayBacked<u32, StackAllocator>&& indices
) {
    Geometry& new_geometry = GeometrySystem::get_empty_slot();
    new_geometry.set_vertices_and_indices(std::move(vertices), std::move(indices));
    return new_geometry;
}

Option<Geometry*> GeometrySystem::get_geometry_by_id(u32 id) {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");

    if (id < 0 || id >= state_ptr->geometries.count()) {
        return {None::VALUE};
    }

    auto& geometry{ state_ptr->geometries[id] };
    geometry.ref_count++;
    if (geometry.generation == INVALID_ID) {
        geometry.generation = 0;
    } else {
        geometry.generation++;
    }
    return &geometry;
}

Geometry& GeometrySystem::get_default_geometry() { 
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");

    auto& default_geometry{ state_ptr->default_geometry };
    default_geometry.ref_count++;
    if (default_geometry.generation == INVALID_ID) {
        default_geometry.generation = 0;
    } else {
        default_geometry.generation++;
    }
    return state_ptr->default_geometry;
}

void GeometrySystem::free_geometry(u32 id) {
    auto& geometry{ state_ptr->geometries[id] };

    if (geometry.ref_count > 0) {
        geometry.ref_count--;
        if (geometry.ref_count < 1 && geometry.auto_release) {
            geometry.destroy();
        }
    }
}

// Mesh (geometry + material)

void Mesh::create_from_existing_data(
    Geometry* geometry,
    Material* material,
    Mesh& out_mesh
) {
    out_mesh.geometry = geometry;
    out_mesh.material = material;
}

void Mesh::set_geometry(Geometry* input_geometry) {
    SF_ASSERT_MSG(geometry && input_geometry, "Should be valid ptrs");
    geometry = input_geometry;
}

void Mesh::init_geometry(
    DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
    DynamicArrayBacked<u32, StackAllocator>&& indices
) {
    SF_ASSERT_MSG(geometry, "Should be valid ptr");
    geometry->set_vertices_and_indices(std::move(vertices), std::move(indices));
}

void Mesh::set_material(Material* input_material) {
    SF_ASSERT_MSG(material && input_material, "Should be valid ptrs");
    material = input_material;
}

// void Mesh::draw(
//     const VulkanDevice& device,
//     VulkanVertexIndexBuffer& vbo,
//     VulkanShaderPipeline& shader,
// ) {
//     vbo.set_geometry(device, *this);
 // shader.
// }

bool Mesh::has_geometry() const {
    return geometry != nullptr;
}

bool Mesh::has_material() const {
    return material != nullptr;    
}

void Mesh::destroy() {
    if (geometry) {
        geometry->destroy();
    }
    if (material) {
        material->destroy();
    }
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

DynamicArrayBacked<Vertex, StackAllocator> GeometrySystem::define_cube_vertices(StackAllocator& allocator) {
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

DynamicArrayBacked<u32, StackAllocator> GeometrySystem::define_cube_indices(StackAllocator& allocator) {
    return std::tuple<StackAllocator*, std::initializer_list<u32>>{
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

