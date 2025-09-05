#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/frame.hpp"
#include "sf_vulkan/device.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanMemory {
    VkDeviceMemory         handle;
    VkMemoryRequirements   requirements;
};
    
struct VulkanBuffer {
public:
    VkBuffer                handle;
    VulkanMemory            memory;
public:
    static void create(const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlagBits usage, VkSharingMode sharing_mode, VkMemoryPropertyFlagBits memory_properties,VulkanBuffer& out_buffer);
    void destroy(const VulkanDevice& device);
};

// TODO: remove when sure that we don't need this
// struct VulkanVertexBuffer {
// public:
//     DynamicArray<Vertex>    vertices;
//     VulkanBuffer            staging_buffer;
//     VulkanBuffer            vertex_buffer;
// public:
//     static bool create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, VulkanVertexBuffer& out_buffer);
//     bool copy_vertices_to_gpu(const VulkanDevice& device);
//     void destroy(const VulkanDevice& device);
// };

// struct VulkanIndexBuffer {
// public:
//     DynamicArray<u16>       indices;
//     VulkanBuffer            staging_buffer;
//     VulkanBuffer            index_buffer;
// public:
//     static bool create(const VulkanDevice& device, DynamicArray<u16>&& indices, VulkanIndexBuffer& out_buffer);
//     bool copy_indices_to_gpu(const VulkanDevice& device);
//     void destroy(const VulkanDevice& device);
// };

struct VulkanCoherentBuffer {
public:
    DynamicArray<VertexOrIndices>    data;
    VulkanBuffer                     staging_buffer;
    VulkanBuffer                     main_buffer;
    u32                              indeces_offset;
    u16                              indeces_count;
public:
    static bool create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, DynamicArray<u16>&& indices, VulkanCoherentBuffer& out_buffer);
    bool copy_data_to_gpu(const VulkanDevice& device);
    void destroy(const VulkanDevice& device);
};

} // sf
