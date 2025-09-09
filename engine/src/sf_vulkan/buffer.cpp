#include "sf_vulkan/buffer.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/swapchain.hpp"
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

bool VulkanCoherentBuffer::create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, DynamicArray<u16>&& indices, VulkanCoherentBuffer& out_buffer) {
    out_buffer.data.resize(vertices.size_in_bytes() + indices.size_in_bytes());
    out_buffer.indeces_count = indices.count();
    out_buffer.indeces_offset = vertices.size_in_bytes(); 
    sf_mem_copy(out_buffer.data.data(), vertices.data(), vertices.size_in_bytes());
    sf_mem_copy(out_buffer.data.data() + out_buffer.indeces_offset, indices.data(), indices.size_in_bytes());

    VulkanBuffer::create(
        device, out_buffer.data.size_in_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        out_buffer.staging_buffer
    );

    VulkanBuffer::create(
        device, out_buffer.data.size_in_bytes(), static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        out_buffer.main_buffer
    );

    return out_buffer.copy_data_to_gpu(device);
}

bool VulkanCoherentBuffer::copy_data_to_gpu(const VulkanDevice& device) {
    void* mapped_gpu_data;
    if (vkMapMemory(device.logical_device, staging_buffer.memory.handle, 0, data.size_in_bytes(), 0, &mapped_gpu_data) != VK_SUCCESS) {
        return false;
    }

    sf_mem_copy(mapped_gpu_data, data.data(), data.size_in_bytes());
    vkUnmapMemory(device.logical_device, staging_buffer.memory.handle);
    return true;
}

void VulkanCoherentBuffer::destroy(const VulkanDevice& device) {
    staging_buffer.destroy(device);
    main_buffer.destroy(device);
}

VulkanUniformBuffersMapped::VulkanUniformBuffersMapped()
{
    ubos.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    mapped_memory.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
}

bool VulkanUniformBuffersMapped::create(const VulkanDevice& device, VulkanUniformBuffersMapped& out_mapped_buffers) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        VulkanBuffer::create(device, sizeof(VulkanUniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
            out_mapped_buffers.buffers[i]
        );  
        if (vkMapMemory(device.logical_device, out_mapped_buffers.buffers[i].memory.handle, 0, sizeof(VulkanUniformBufferObject), 0, &out_mapped_buffers.mapped_memory[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

void VulkanUniformBuffersMapped::destroy(const VulkanDevice& device) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers[i].destroy(device);
    }
}

} // sf
