#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_vulkan/shared_types.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_vulkan/mesh.hpp"

namespace sf {

struct Model {
    DynamicArrayBacked<Mesh, ArenaAllocator, false> meshes;
    static void create(ArenaAllocator& alloc, Model& out_model);
    static bool load(std::string_view file_name, ArenaAllocator& main_alloc, StackAllocator& alloc, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, Model& out_model);
};

} // sf
