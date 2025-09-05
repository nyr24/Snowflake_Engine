#pragma once

#include "sf_containers/dynamic_array.hpp"
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

struct VulkanVertexBuffer {
public:
    DynamicArray<Vertex>    vertices;
    VulkanBuffer            staging_buffer;
    VulkanBuffer            vertex_buffer;
public:
    static bool create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, VulkanVertexBuffer& out_buffer);
    bool copy_vertices_to_gpu(const VulkanDevice& device);
    void destroy(const VulkanDevice& device);
};

struct VulkanIndexBuffer {
public:
    DynamicArray<u16>       indices;
    VulkanBuffer            staging_buffer;
    VulkanBuffer            index_buffer;
public:
    static bool create(const VulkanDevice& device, DynamicArray<u16>&& indices, VulkanIndexBuffer& out_buffer);
    bool copy_indices_to_gpu(const VulkanDevice& device);
    void destroy(const VulkanDevice& device);
};

} // sf
