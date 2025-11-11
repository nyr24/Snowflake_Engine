#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texture_coord;

    static VkVertexInputBindingDescription get_binding_descr();
};

struct VulkanShaderPipeline;

struct Mesh {
    static constexpr u32 MAX_VERTEX_COUNT{ 4 * 256 };
    static constexpr u32 MAX_INDEX_COUNT{ 6 * 256 };
    DynamicArrayBacked<u8, ArenaAllocator> data;
    u32  indeces_offset;
    u32  indeces_count;
    u32  id;
    u32  generation;
    u32  ref_count;
    bool auto_release;
public:
    static u32 consteval get_memory_requirement() { return sizeof(Vertex) * MAX_VERTEX_COUNT + sizeof(u16) * MAX_INDEX_COUNT; };
    void init_from_vertices_and_indices(DynamicArrayBacked<Vertex, StackAllocator>&& vertices, DynamicArrayBacked<u16, StackAllocator>&& indices);
    void destroy();
};

struct MeshSystemState {
    static constexpr u32 MESH_INIT_CAPACITY{64};
    DynamicArrayBacked<Mesh, ArenaAllocator> meshes;
    Mesh                                     default_mesh;
public:
    static consteval u32 get_memory_requirement() { return MESH_INIT_CAPACITY * sizeof(Mesh) + MESH_INIT_CAPACITY * Mesh::get_memory_requirement(); }
    static void create(ArenaAllocator& allocator, StackAllocator& temp_allocator, MeshSystemState& out_state);
private:
    void create_default_mesh(StackAllocator& temp_allocator);
    static DynamicArrayBacked<Vertex, StackAllocator> define_cube_vertices(StackAllocator& temp_allocator);
    static DynamicArrayBacked<u16, StackAllocator> define_cube_indices(StackAllocator& temp_allocator);
};

Mesh* mesh_system_get_mesh(u32 id);
Mesh* mesh_system_get_default_mesh();
void mesh_system_free_mesh(u32 id);

} // sf
