#pragma once

#include "sf_core/defines.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;

struct VulkanImage {
public:
    VkImage         handle;
    VkDeviceMemory  memory;
    VkImageView     view;
    u32             width;
    u32             height;

public:
    static bool create(
        const VulkanContext& context,
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

    void create_view(
        const VulkanContext& context,
        VkFormat format,
        VkImageAspectFlags aspect_flags
    );

    void destroy(const VulkanContext& context);
};

} // sf
