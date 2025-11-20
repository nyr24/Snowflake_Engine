#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/shared_types.hpp"
#include "sf_containers/optional.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace sf {

struct Vertex {
    using IndexType = u32;
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texture_coord;

    static VkVertexInputBindingDescription get_binding_descr();
};

struct Geometry {
public:
    static constexpr u32 MAX_VERTEX_COUNT{ 4 * 256 };
    static constexpr u32 MAX_INDEX_COUNT{ 6 * 256 };

    DynamicArrayBacked<u8, ArenaAllocator, false> data;
    u32 indeces_offset;
    u32 indeces_count;
    u32 id;
    u32 generation;
    u16 ref_count;
    bool auto_release;
public:
    static u32 consteval get_memory_requirement() { return sizeof(Vertex) * MAX_VERTEX_COUNT + sizeof(Vertex::IndexType) * MAX_INDEX_COUNT; };
    static void create_empty(
        u32 id,
        ArenaAllocator& alloc,
        Geometry& out_geometry
    );
    static void create_from_vertices_and_indices(
        u32 id,
        ArenaAllocator& alloc,
        DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
        DynamicArrayBacked<u32, StackAllocator>&& indices,
        Geometry& out_geometry
    );
    void set_vertices_and_indices(
        DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
        DynamicArrayBacked<u32, StackAllocator>&& indices
    );
    void destroy();
};

struct GeometrySystem {
    static constexpr u32 INIT_CAPACITY{64};
    DynamicArrayBacked<Geometry, ArenaAllocator, false>  geometries;
    Geometry                                             default_geometry;
    ArenaAllocator*                                      alloc;
public:
    static consteval u32 get_memory_requirement() { return INIT_CAPACITY * sizeof(Geometry) + INIT_CAPACITY * Geometry::get_memory_requirement(); }
    static void create(ArenaAllocator& allocator, StackAllocator& temp_allocator, GeometrySystem& out_state);
    static Option<Geometry*> get_geometry_by_id(u32 id);
    static Geometry& create_geometry_and_get(
        DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
        DynamicArrayBacked<u32, StackAllocator>&& indices
    );
    static Geometry& get_default_geometry();
    static void free_geometry(u32 id);
    static Geometry& get_empty_slot();
private:
    void create_default_geometry(StackAllocator& temp_allocator);
    static DynamicArrayBacked<Vertex, StackAllocator> define_cube_vertices(StackAllocator& temp_allocator);
    static DynamicArrayBacked<u32, StackAllocator> define_cube_indices(StackAllocator& temp_allocator);
};

struct Mesh {
    static constexpr u32 MAX_TEXTURE_COUNT{ 8 };
    Geometry* geometry;
    Material* material;
public:
    static void create_from_existing_data(
        Geometry* geometry,
        Material* material,
        Mesh& out_mesh
    );
    void init_geometry(
        DynamicArrayBacked<Vertex, StackAllocator>&& vertices,
        DynamicArrayBacked<u32, StackAllocator>&& indices
    );
    void set_geometry(Geometry* geometry);
    void set_material(Material* mat);
    bool has_material() const;
    bool has_geometry() const;
    void draw();
    void destroy();
};

} // sf
