#include "sf_vulkan/buffer.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool VulkanBuffer::create(const VulkanDevice& device, DynamicArray<Vertex>&& geometry_in, Type type, VulkanBuffer& out_buffer) {
    out_buffer.geometry = std::move(geometry_in);
    out_buffer.type = type;

    VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = out_buffer.geometry.size_in_bytes(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateBuffer(device.logical_device, &create_info, nullptr, &out_buffer.handle));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device.logical_device, out_buffer.handle, &memory_requirements);
    Option<u32> memory_index = device.find_memory_index(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memory_index.is_none()) {
        panic("Memory index for vertex buffer was not found");
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_index.unwrap(),
    };

    // TODO: custom allocator
    sf_vk_check(vkAllocateMemory(device.logical_device, &alloc_info, nullptr, &out_buffer.memory));
    sf_vk_check(vkBindBufferMemory(device.logical_device, out_buffer.handle, out_buffer.memory, 0));

    if (!out_buffer.copy_geometry_to_gpu(device)) {
        LOG_ERROR("Failed to copy geometry to the gpu");
        return false;
    }
    
    return true;
}

void VulkanBuffer::destroy(const VulkanDevice& device) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyBuffer(device.logical_device, handle, nullptr);
        handle = nullptr;
    }    
    if (memory) {
        // TODO: custom allocator
        vkFreeMemory(device.logical_device, memory, nullptr);
        memory = nullptr;
    }
}

bool VulkanBuffer::copy_geometry_to_gpu(const VulkanDevice& device) {
    void* res{nullptr};
    if (vkMapMemory(device.logical_device, memory, 0, geometry.size_in_bytes(), 0, &res) != VK_SUCCESS) {
        return false;
    }
    sf_mem_copy(res, geometry.data(), geometry.size_in_bytes());
    vkUnmapMemory(device.logical_device, memory);
    return true;
}

VkBufferUsageFlagBits VulkanBuffer::map_type_to_usage_flag(VulkanBuffer::Type type) {
    switch (type) {
        case VulkanBuffer::Type::VERTEX: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case VulkanBuffer::Type::UNIFORM: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case VulkanBuffer::Type::INDEX: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    };

    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
}

} // sf

