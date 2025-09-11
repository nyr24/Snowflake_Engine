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

    static void allocate(VulkanContext& context, VkCommandPool command_pool, std::span<VulkanCommandBuffer> out_buffers, bool is_primary);
    void begin_recording(VulkanContext& context, VkCommandBufferUsageFlags begin_flags);
    void end_recording(VulkanContext& context);
    void reset(VulkanContext& context);
    void submit(VulkanContext& context, VkQueue queue, VkSubmitInfo& submit_info, Option<VulkanFence> fence);
    void free(VulkanContext& context, VkCommandPool command_pool);
    void transition_image_layout(
        VulkanContext& context,
        uint32_t image_index,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkPipelineStageFlags2 src_stage_mask,
        VkAccessFlags2 src_access_mask,
        VkPipelineStageFlags2 dst_stage_mask,
        VkAccessFlags2 dst_access_mask
    );
    void copy_buffer_data(VkBuffer src, VkBuffer dst, VkBufferCopy* copy_region);
    void record_draw_commands(VulkanContext& context, VulkanShaderPipeline& curr_pipeline, u32 image_index);
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
