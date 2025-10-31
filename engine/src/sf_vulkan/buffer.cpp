#include "sf_vulkan/buffer.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include <cstddef>
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
        LOG_ERROR("Memory index for vertex buffer was not found");
        return;
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

bool VulkanBuffer::copy_data(const VulkanDevice& device, void* data, u32 byte_size) {
    void* mapped_data;

    if (vkMapMemory(device.logical_device, memory.handle, 0, byte_size, 0, &mapped_data) != VK_SUCCESS) {
        return false;
    }

    sf_mem_copy(mapped_data, data, byte_size);
    vkUnmapMemory(device.logical_device, memory.handle);
    return true;
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

bool VulkanVertexIndexBuffer::create(const VulkanDevice& device, Mesh&& mesh, LinearAllocator& render_system_allocator, VulkanVertexIndexBuffer& out_buffer) {
    out_buffer.data.set_allocator(&render_system_allocator);
    out_buffer.data.resize(mesh.vertices.size_in_bytes() + mesh.indices.size_in_bytes());
    out_buffer.indeces_count = mesh.indices.count();
    out_buffer.indeces_offset = mesh.vertices.size_in_bytes(); 
    sf_mem_copy(out_buffer.data.data(), mesh.vertices.data(), mesh.vertices.size_in_bytes());
    sf_mem_copy(out_buffer.data.data() + out_buffer.indeces_offset, mesh.indices.data(), mesh.indices.size_in_bytes());

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

    return out_buffer.staging_buffer.copy_data(device, out_buffer.data.data(), out_buffer.data.size_in_bytes());
}

void VulkanVertexIndexBuffer::destroy(const VulkanDevice& device) {
    staging_buffer.destroy(device);
    main_buffer.destroy(device);
}

void VulkanVertexIndexBuffer::bind(const VulkanCommandBuffer& cmd_buffer) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer.handle, 0, 1, &main_buffer.handle, offsets);
    vkCmdBindIndexBuffer(cmd_buffer.handle, main_buffer.handle, indeces_offset, VK_INDEX_TYPE_UINT16);
}

void VulkanVertexIndexBuffer::draw(const VulkanCommandBuffer& cmd_buffer) {
    vkCmdDrawIndexed(cmd_buffer.handle, indeces_count, 1, 0, 0, 0);
}

bool VulkanGlobalUniformBufferObject::create(const VulkanDevice& device, VulkanGlobalUniformBufferObject& out_global_ubo) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        VulkanBuffer::create(device, sizeof(GlobalUniformObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
            out_global_ubo.buffers[i]
        );  
        if (vkMapMemory(device.logical_device, out_global_ubo.buffers[i].memory.handle, 0, sizeof(GlobalUniformObject), 0, &out_global_ubo.mapped_memory[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

VulkanGlobalUniformBufferObject::VulkanGlobalUniformBufferObject() {
    global_ubos.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    buffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    mapped_memory.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
}

// NOTE: maybe dangerous to update current frame
void VulkanGlobalUniformBufferObject::update(u32 curr_frame, const glm::mat4& view, const glm::mat4& proj) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        global_ubos[i].view = view;
        global_ubos[i].proj = proj;
        sf_mem_copy(mapped_memory[i], &global_ubos[i], sizeof(GlobalUniformObject));
    }
}

void VulkanGlobalUniformBufferObject::update_view(const glm::mat4& view) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        global_ubos[i].view = view;
        sf_mem_copy(mapped_memory[i], &global_ubos[i], sizeof(glm::mat4));
    }
}

void VulkanGlobalUniformBufferObject::update_proj(const glm::mat4& proj) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        global_ubos[i].proj = proj;
        sf_mem_copy(static_cast<u8*>(mapped_memory[i]) + offsetof(GlobalUniformObject, proj), reinterpret_cast<u8*>(&global_ubos[i]) + offsetof(GlobalUniformObject, proj), sizeof(glm::mat4));
    }
}

void VulkanGlobalUniformBufferObject::destroy(const VulkanDevice& device) {
    for (u32 i{0}; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        buffers[i].destroy(device);
        vkUnmapMemory(device.logical_device, buffers[i].memory.handle);
    }
}

bool VulkanLocalUniformBufferObject::create(const VulkanDevice& device, VulkanLocalUniformBufferObject& out_local_ubo) {
    VulkanBuffer::create(device, sizeof(LocalUniformObject) * MAX_OBJECT_COUNT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE, static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        out_local_ubo.buffer
    );  
    if (vkMapMemory(device.logical_device, out_local_ubo.buffer.memory.handle, 0, sizeof(LocalUniformObject), 0, &out_local_ubo.mapped_memory) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void VulkanLocalUniformBufferObject::update(u32 offset, glm::vec4 diffuse_color) {
    uniform_object.diffuse_color = diffuse_color;
    sf_mem_copy(ptr_step_bytes_forward(mapped_memory, offset), &uniform_object, sizeof(LocalUniformObject));
}

void VulkanLocalUniformBufferObject::destroy(const VulkanDevice& device) {
    buffer.destroy(device);
    if (buffer.memory.handle) {
        vkUnmapMemory(device.logical_device, buffer.memory.handle);
        buffer.memory.handle = nullptr;
    }
}

} // sf
