#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/synch.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;

enum struct VulkanCommandBufferState {
    NOT_ALLOCATED,
    READY,
    RECORDING_BEGIN,
    RECORDING_END,
    SUBMITTED,
};

struct VulkanCommandBuffer {
public:
    VkCommandBuffer             handle;
    VulkanCommandBufferState    state;

public:
    VulkanCommandBuffer();

    static void allocate(VulkanContext& context, VkCommandPool command_pool,  DynamicArray<VulkanCommandBuffer>& out_buffers, bool is_primary);
    // static VulkanCommandBuffer begin_single_use(VulkanContext& context, VkCommandPool command_pool, bool is_primary);
    void begin_recording(VulkanContext& context, VkCommandBufferUsageFlags begin_flags, u32 image_index);
    void end(VulkanContext& context);
    void reset(VulkanContext& context);
    void submit(VulkanContext& context, VkQueue queue, VkSubmitInfo& submit_info, VulkanFence& fence);
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

private:
    void begin_rendering(VulkanContext& context, u32 image_index);
};

enum struct VulkanCommandPoolType {
    GRAPHICS
};

struct VulkanCommandPool {
public:
    VkCommandPool           handle;
    VulkanCommandPoolType   type;

public:
    static void create(VulkanContext& context, VulkanCommandPoolType type, u32 queue_family_index, VulkanCommandPool& out_pool);
    void destroy(VulkanContext& context);
};

} // sf
