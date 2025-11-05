#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/mesh.hpp"
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
    static void create(const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlagBits usage, VkSharingMode sharing_mode, VkMemoryPropertyFlagBits memory_properties, VulkanBuffer& out_buffer);
    bool copy_data(const VulkanDevice& device, void* data, u32 byte_size);
    void destroy(const VulkanDevice& device);
};

struct VulkanVertexIndexBuffer {
public:
    DynamicArray<u8, LinearAllocator>    data;
    VulkanBuffer                         staging_buffer;
    VulkanBuffer                         main_buffer;
    u32                                  indeces_offset;
    u16                                  indeces_count;
public:
    static bool create(const VulkanDevice& device, Mesh&& mesh, LinearAllocator& render_system_allocator, VulkanVertexIndexBuffer& out_buffer);
    bool copy_data_to_staging_buffer(const VulkanDevice& device);
    void destroy(const VulkanDevice& device);
    void bind(const VulkanCommandBuffer& cmd_buffer);
    void draw(const VulkanCommandBuffer& cmd_buffer);
};

// Needs to be at least 256 bytes for Nvidia cards
struct GlobalUniformObject {
    glm::mat4    view;
    glm::mat4    proj;
    glm::mat4    reserved_0;
    glm::mat4    reserved_1;
};

struct LocalUniformObject {
    glm::vec4 diffuse_color;
    glm::vec4 reserved_0;
    glm::vec4 reserved_1;
    glm::vec4 reserved_2;
};

struct VulkanGlobalUniformBufferObject {
    FixedArray<GlobalUniformObject, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>       global_ubos;
    FixedArray<VulkanBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>              buffers;
    FixedArray<void*, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>                     mapped_memory;

    VulkanGlobalUniformBufferObject();
    static bool create(const VulkanDevice& device, VulkanGlobalUniformBufferObject& out_global_ubo);
    void destroy(const VulkanDevice& device);
    void update(u32 curr_frame, const glm::mat4& view, const glm::mat4& proj);
    void update_view(const glm::mat4& view);
    void update_proj(const glm::mat4& proj);
};

// THINK: do we need this per frame also?
struct VulkanLocalUniformBufferObject {
    LocalUniformObject        uniform_object;
    VulkanBuffer              buffer;
    void*                     mapped_memory;

    // VulkanLocalUniformBufferObject();
    static bool create(const VulkanDevice& device, VulkanLocalUniformBufferObject& out_local_ubo);
    void destroy(const VulkanDevice& device);
    void update(u32 offset, glm::vec4 diffuse_color);
};

struct VulkanPushConstantBlock {
    glm::mat4    model;
    void update(const glm::mat4& model_new);
};

} // sf
