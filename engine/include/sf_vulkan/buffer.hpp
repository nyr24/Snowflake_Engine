#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/frame.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/swapchain.hpp"
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

struct VulkanCoherentBuffer {
public:
    DynamicArray<u8>                 data;
    VulkanBuffer                     staging_buffer;
    VulkanBuffer                     main_buffer;
    u32                              indeces_offset;
    u16                              indeces_count;
public:
    static bool create(const VulkanDevice& device, DynamicArray<Vertex>&& vertices, DynamicArray<u16>&& indices, VulkanCoherentBuffer& out_buffer);
    bool copy_data_to_gpu(const VulkanDevice& device);
    void destroy(const VulkanDevice& device);
};

struct VulkanUniformBufferObject {
    glm::mat4    model;
    glm::mat4    view;
    glm::mat4    proj;
};

struct VulkanUniformBuffersMapped {
    // THINK: maybe should use array of structures
    FixedArray<VulkanUniformBufferObject, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT> ubos;  
    FixedArray<VulkanBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>              buffers;
    FixedArray<void*, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>                     mapped_memory;  

    VulkanUniformBuffersMapped();
    static bool create(const VulkanDevice& device, VulkanUniformBuffersMapped& out_mapped_buffers);
    void destroy(const VulkanDevice& device);
};

} // sf
