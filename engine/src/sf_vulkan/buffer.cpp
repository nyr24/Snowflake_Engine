#include "sf_vulkan/buffer.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

void VulkanBuffer::create(
    const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlagBits usage,
    VkSharingMode sharing_mode, VkMemoryPropertyFlagBits memory_properties, VulkanBuffer& out_buffer
)
{
    VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = sharing_mode,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateBuffer(device.logical_device, &create_info, nullptr, &out_buffer.handle));

    vkGetBufferMemoryRequirements(device.logical_device, out_buffer.handle, &out_buffer.memory.requirements);
    Option<u32> memory_index = device.find_memory_index(out_buffer.memory.requirements.memoryTypeBits, memory_properties);

    if (memory_index.is_none()) {
        panic("Memory index for vertex buffer was not found");
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = out_buffer.memory.requirements.size,
        .memoryTypeIndex = memory_index.unwrap(),
    };

    // TODO: custom allocator
    sf_vk_check(vkAllocateMemory(device.logical_device, &alloc_info, nullptr, &out_buffer.memory.handle));
    sf_vk_check(vkBindBufferMemory(device.logical_device, out_buffer.handle, out_buffer.memory.handle, 0));
}

void VulkanBuffer::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyBuffer(device.logical_device, handle, nullptr);
        handle = nullptr;
    }    
    if (memory.handle) {
        // TODO: custom allocator
        vkFreeMemory(device.logical_device, memory.handle, nullptr);
        memory.handle = nullptr;
    }
}
bool VulkanVertexBuffer::create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, VulkanVertexBuffer& out_buffer) {
    out_buffer.vertices = std::move(vertices);

    VulkanBuffer::create(
        device, out_buffer.vertices.size_in_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        out_buffer.staging_buffer
    );

    out_buffer.copy_vertices_to_gpu(device);

    VulkanBuffer::create(
        device, out_buffer.vertices.size_in_bytes(), static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        out_buffer.vertex_buffer
    );

    return out_buffer.copy_vertices_to_gpu(device);
}

bool VulkanVertexBuffer::copy_vertices_to_gpu(const VulkanDevice& device) {
    void* mapped_gpu_data;
    if (vkMapMemory(device.logical_device, staging_buffer.memory.handle, 0, vertices.size_in_bytes(), 0, &mapped_gpu_data) != VK_SUCCESS) {
        return false;
    }

    sf_mem_copy(mapped_gpu_data, vertices.data(), vertices.size_in_bytes());
    vkUnmapMemory(device.logical_device, staging_buffer.memory.handle);
    return true;
}

void VulkanVertexBuffer::destroy(const VulkanDevice& device) {
    staging_buffer.destroy(device);
    vertex_buffer.destroy(device);
}

bool VulkanIndexBuffer::create(const VulkanDevice& device, DynamicArray<u16>&& indices, VulkanIndexBuffer& out_buffer) {
    out_buffer.indices = std::move(indices);

    VulkanBuffer::create(
        device, out_buffer.indices.size_in_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        out_buffer.staging_buffer
    );

    out_buffer.copy_indices_to_gpu(device);

    VulkanBuffer::create(
        device, out_buffer.indices.size_in_bytes(), static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        out_buffer.index_buffer
    );

    return out_buffer.copy_indices_to_gpu(device);
}

bool VulkanIndexBuffer::copy_indices_to_gpu(const VulkanDevice& device) {
    void* mapped_gpu_data;
    if (vkMapMemory(device.logical_device, staging_buffer.memory.handle, 0, indices.size_in_bytes(), 0, &mapped_gpu_data) != VK_SUCCESS) {
        return false;
    }

    sf_mem_copy(mapped_gpu_data, indices.data(), indices.size_in_bytes());
    vkUnmapMemory(device.logical_device, staging_buffer.memory.handle);
    return true;
}

void VulkanIndexBuffer::destroy(const VulkanDevice& device) {
    staging_buffer.destroy(device);
    index_buffer.destroy(device);
}

} // sf

