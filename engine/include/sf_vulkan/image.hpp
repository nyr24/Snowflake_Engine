#pragma once

#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

void image_create(
    VulkanContext& context,
    VkImageType type,
    u32 width,
    u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_flags,
    bool create_view,
    VkImageAspectFlags aspect_flags,
    VulkanImage& out_image
);

void image_view_create(
    VulkanContext& context,
    VkFormat format,
    VulkanImage& image,
    VkImageAspectFlags aspect_flags
);

void image_destroy(
    VulkanContext& context,
    VulkanImage& image
);

} // sf
