#include "sf_vulkan/mesh.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/pipeline.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

static GeometrySystem* state_ptr{nullptr};

void GeometryView::create(
    u32           indeces_offset_,
    u32           indeces_count_,
    u32           vertex_offset_,
    GeometryView& out_view
) {
    out_view.indeces_count = indeces_count_;
    out_view.indeces_offset = indeces_offset_;
    out_view.vertex_offset = vertex_offset_;
}

void GeometryView::destroy() {
    indeces_count = INVALID_ID;
    indeces_offset = INVALID_ID;
    vertex_offset = INVALID_ID;
}

// GeometrySystem

void GeometrySystem::create(ArenaAllocator& main_allocator, StackAllocator& temp_allocator, GeometrySystem &out_state) {
    state_ptr = &out_state;
    out_state.alloc = &main_allocator;
    out_state.geometry_views.set_allocator(&main_allocator);
    out_state.geometry_views.reserve(INIT_GEOMETRY_COUNT);
    out_state.indices.set_allocator(&main_allocator);
    out_state.indices.reserve(INIT_GEOMETRY_COUNT * AVG_INDEX_COUNT);
    out_state.vertices.set_allocator(&main_allocator);
    out_state.vertices.reserve(INIT_GEOMETRY_COUNT * AVG_VERTEX_COUNT);

    GeometrySystem::create_geometry_and_get_view(GeometrySystem::define_cube_vertices(temp_allocator), GeometrySystem::define_cube_indices(temp_allocator));
}

GeometryView& GeometrySystem::create_geometry_and_get_view(
    DynamicArray<Vertex, StackAllocator>&& vertices_input,
    DynamicArray<u32, StackAllocator>&& indices_input
) { 
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");

    auto& vertices = state_ptr->vertices;
    auto& indices = state_ptr->indices;
    
    u32 vert_offset = vertices.count();
    u32 vert_remain_cap = vertices.capacity_remain();
    u32 index_offset = indices.count();
    u32 index_remain_cap = indices.capacity_remain();

    u32 vert_input_cnt = vertices_input.count();
    if (vert_input_cnt > vert_remain_cap) {
        vertices.reserve_exponent(vert_input_cnt - vert_remain_cap);   
    }
    u32 index_input_cnt = indices_input.count();
    if (index_input_cnt > index_remain_cap) {
        indices.reserve_exponent(index_input_cnt - index_remain_cap);   
    }

    vertices.append_slice(vertices_input.to_span());
    indices.append_slice(indices_input.to_span());

    GeometryView new_view;
    // NOTE: THINK maybe should be in bytes, not elements?
    // GeometryView::create(index_offset * sizeof(u32), indices_input.count(), vert_offset * sizeof(Vertex), new_view);
    GeometryView::create(index_offset, indices_input.count(), vert_offset, new_view);
    state_ptr->geometry_views.append(new_view);

    return state_ptr->geometry_views.last();
}

std::span<Vertex> GeometrySystem::get_vertices() {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");
    return state_ptr->vertices.to_span(); 
}

std::span<Vertex::IndexType> GeometrySystem::get_indices() {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");
    return state_ptr->indices.to_span();
}

GeometryView& GeometrySystem::get_default_geometry_view() { 
    SF_ASSERT_MSG(state_ptr && state_ptr->geometry_views.count() > 0, "Should have the default geometry view");
    return state_ptr->geometry_views.first();
}

// Mesh

void Mesh::create_empty(VulkanShaderPipeline& shader, const VulkanDevice& device, Mesh& out_mesh) {
    out_mesh.descriptor_state_index = shader.acquire_resouces(device);
}

void Mesh::create_from_existing_data(
    VulkanShaderPipeline& shader,
    const VulkanDevice& device,
    GeometryView* geometry_view,
    Material* material,
    Mesh& out_mesh
) {
    out_mesh.descriptor_state_index = shader.acquire_resouces(device);
    out_mesh.geometry_view = geometry_view;
    out_mesh.material = material;
}

void Mesh::set_geometry_view(GeometryView* input_geometry_view) {
    SF_ASSERT_MSG(input_geometry_view, "Should be valid ptrs");
    geometry_view = input_geometry_view;
}

void Mesh::set_material(Material* input_material) {
    SF_ASSERT_MSG(input_material, "Should be valid ptrs");
    material = input_material;
}

void Mesh::draw(VulkanCommandBuffer& cmd_buffer) {
    vkCmdDrawIndexed(cmd_buffer.handle, geometry_view->indeces_count, 1, geometry_view->indeces_offset, geometry_view->vertex_offset, 0); 
}

bool Mesh::has_geometry_view() const {
    return geometry_view != nullptr;
}

bool Mesh::has_material() const {
    return material != nullptr;    
}

bool Mesh::valid() const {
    return geometry_view && material && descriptor_state_index != INVALID_ID;
}

void Mesh::destroy() {
    if (geometry_view) {
        geometry_view->destroy();
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

DynamicArray<Vertex, StackAllocator> GeometrySystem::define_cube_vertices(StackAllocator& allocator) {
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

DynamicArray<u32, StackAllocator> GeometrySystem::define_cube_indices(StackAllocator& allocator) {
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

