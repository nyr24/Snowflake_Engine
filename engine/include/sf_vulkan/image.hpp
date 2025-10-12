#pragma once

#include "sf_core/defines.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanDevice;
struct VulkanCommandBuffer;

struct VulkanImage {
public:
    VkImage         handle;
    VkDeviceMemory  memory;
    VkImageView     view;
    u32             width;
    u32             height;

public:
    static bool create(
        const VulkanDevice& device,
        VulkanImage& out_image,
        VkImageType type,
        u32 width,
        u32 height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memory_flags,
        bool create_view,
        VkImageAspectFlags aspect_flags,
        VkImageLayout image_init_layout = VK_IMAGE_LAYOUT_UNDEFINED
    );

    static void transition_layout(
        VkImage image,
        const VulkanCommandBuffer& cmd_buffer,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkPipelineStageFlags2 src_stage_mask,
        VkAccessFlags2 src_access_mask,
        VkPipelineStageFlags2 dst_stage_mask,
        VkAccessFlags2 dst_access_mask
    );

    static VkImageView create_view(
        VkImage image,
        const VulkanDevice& device,
        VkFormat format,
        VkImageAspectFlags aspect_flags
    );

    void destroy(const VulkanDevice& device);
};

} // sf
