#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/shared_types.hpp"
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

struct GeometryView {
public:
    u32  indeces_offset;
    u32  indeces_count;
    u32  vertex_offset;
public:
    static void create(
        u32  indeces_offset,
        u32  indeces_count,
        u32  vertex_offset,
        GeometryView& out_view
    );
    void draw(VulkanCommandBuffer& cmd_buffer);
    void destroy();
};

struct GeometrySystem {
    static constexpr u32 INIT_GEOMETRY_COUNT{64};
    static constexpr u32 AVG_VERTEX_COUNT{ 4 * 256 };
    static constexpr u32 AVG_INDEX_COUNT{ 6 * 256 };

    DynamicArray<Vertex, ArenaAllocator, false>               vertices;
    DynamicArray<Vertex::IndexType, ArenaAllocator, false>    indices;
    DynamicArray<GeometryView, ArenaAllocator, false>         geometry_views;
    ArenaAllocator*                                           alloc;
public:
    static consteval u32 get_memory_requirement() { return INIT_GEOMETRY_COUNT * sizeof(GeometryView) + INIT_GEOMETRY_COUNT * (AVG_INDEX_COUNT * sizeof(Vertex::IndexType) + AVG_VERTEX_COUNT * sizeof(Vertex)); }
    static void create(ArenaAllocator& allocator, StackAllocator& temp_allocator, GeometrySystem& out_state);
    static GeometryView& create_geometry_and_get_view(
        DynamicArray<Vertex, StackAllocator>&& vertices,
        DynamicArray<u32, StackAllocator>&& indices
    );
    static GeometryView& get_default_geometry_view();
    static std::span<Vertex> get_vertices();
    static std::span<Vertex::IndexType> get_indices();
private:
    static DynamicArray<Vertex, StackAllocator> define_cube_vertices(StackAllocator& temp_allocator);
    static DynamicArray<u32, StackAllocator> define_cube_indices(StackAllocator& temp_allocator);
};

struct Mesh {
    static constexpr u32 MAX_TEXTURE_COUNT{ 8 };
    GeometryView* geometry_view;
    Material*     material;
    u32           descriptor_state_index{INVALID_ID};
public:
    static void create_empty(VulkanShaderPipeline& shader, const VulkanDevice& device, Mesh& out_mesh);
    static void create_from_existing_data(
        VulkanShaderPipeline& shader,
        const VulkanDevice& device,
        GeometryView* geometry,
        Material* material,
        Mesh& out_mesh
    );
    void set_geometry_view(GeometryView* geometry);
    bool has_geometry_view() const;
    void set_material(Material* mat);
    bool has_material() const;
    bool valid() const;
    void draw(VulkanCommandBuffer& cmd_buffer);
    void destroy();
};

} // sf
