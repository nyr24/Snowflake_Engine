#pragma once

#include "sf_containers/optional.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/synch.hpp"
#include <span>
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;
struct VulkanShaderPipeline;

enum struct VulkanCommandBufferState {
    NOT_ALLOCATED,
    READY,
    RECORDING_BEGIN,
    RECORDING_END,
    SUBMITTED,
};

struct VulkanCommandBuffer {
public:
    static constexpr u32        MAX_BUFFER_ALLOC_COUNT{20};
    VkCommandBuffer             handle;
    VulkanCommandBufferState    state;

public:
    VulkanCommandBuffer();

    static void allocate(const VulkanDevice& device, VkCommandPool command_pool, std::span<VulkanCommandBuffer> out_buffers, bool is_primary);
    static void allocate_and_begin_single_use(const VulkanDevice& device, VkCommandPool command_pool, VulkanCommandBuffer& out_buffer);
    void begin_recording(VkCommandBufferUsageFlags begin_flags);
    void end_recording();
    void begin_rendering(const VulkanContext& context);
    void end_rendering(const VulkanContext& context);
    void end_single_use(const VulkanContext& context, VkCommandPool command_pool);
    void reset();
    void submit(const VulkanContext& context, VkQueue queue, VkSubmitInfo& submit_info, Option<VulkanFence> fence);
    void free(const VulkanDevice& device, VkCommandPool command_pool);
    void copy_data_between_buffers(VkBuffer src, VkBuffer dst, VkBufferCopy& copy_region);
    void copy_data_from_buffer_to_image(VkBuffer src_buffer, VulkanImage& dst_image, VkImageLayout dst_image_layout) const;
};

enum struct VulkanCommandPoolType {
    GRAPHICS,
    TRANSFER
};

struct VulkanCommandPool {
public:
    VkCommandPool           handle;
    VulkanCommandPoolType   type;

public:
    static void create(VulkanContext& context, VulkanCommandPoolType type, u32 queue_family_index, VkCommandPoolCreateFlagBits create_flags, VulkanCommandPool& out_pool);
    void destroy(VulkanContext& context);
};

} // sf
