#include "sf_vulkan/image.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/device.hpp"
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
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    bool create_view,
    VkImageAspectFlags aspect_flags,
    VulkanImage& out_image
) {
    out_image.width = width;
    out_image.height = height;

    VkImageCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.extent.width = width;
    create_info.extent.height = height;
    create_info.extent.depth = 1;
    create_info.mipLevels = 4;
    create_info.arrayLayers = 1;
    create_info.format = format;
    create_info.tiling = tiling;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    create_info.usage = usage;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // TODO: custom allocator
    // sf_vk_check(vkCreateImage(context.device.logical_device, &create_info, &context.allocator, &out_image.handle));
    sf_vk_check(vkCreateImage(context.device.logical_device, &create_info, nullptr, &out_image.handle));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context.device.logical_device, out_image.handle, &memory_requirements);

    Option<u32> memory_type = find_memory_index(context, memory_requirements.memoryTypeBits, memory_flags);
    if (memory_type.is_none()) {
        LOG_ERROR("Required memory type not found. Image not valid.");
        return;
    }

    VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = memory_type.unwrap_copy();
    // TODO: custom allocator
    // sf_vk_check(vkAllocateMemory(context.device.logical_device, &allocate_info, &context.allocator, &out_image.memory));
    sf_vk_check(vkAllocateMemory(context.device.logical_device, &allocate_info, nullptr, &out_image.memory));

    sf_vk_check(vkBindImageMemory(context.device.logical_device, out_image.handle, out_image.memory, 0));

    if (create_view) {
        out_image.view = nullptr;
        image_view_create(context, format, out_image, aspect_flags);
    }
}

void image_view_create(
    VulkanContext& context,
    VkFormat format,
    VulkanImage& image,
    VkImageAspectFlags aspect_flags
) {
    VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create_info.image = image.handle;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;
    create_info.subresourceRange.aspectMask = aspect_flags;

    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 0;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    // TODO: custom allocator
    // sf_vk_check(vkCreateImageView(context.device.logical_device, &create_info, &context.allocator, &image.view));
    sf_vk_check(vkCreateImageView(context.device.logical_device, &create_info, nullptr, &image.view));
}

void image_destroy(
    VulkanContext& context,
    VulkanImage& image
) {
    if (image.view) {
        // TODO: custom allocator
        // vkDestroyImageView(context.device.logical_device, image.view, &context.allocator);
        vkDestroyImageView(context.device.logical_device, image.view, nullptr);
        image.view = nullptr;
    }
    if (image.memory) {
        // TODO: custom allocator
        // vkFreeMemory(context.device.logical_device, image.memory, &context.allocator);
        vkFreeMemory(context.device.logical_device, image.memory, nullptr);
        image.memory = nullptr;
    }
    if (image.handle) {
        // TODO: custom allocator
        // vkDestroyImage(context.device.logical_device, image.handle, &context.allocator);
        vkDestroyImage(context.device.logical_device, image.handle, nullptr);
        image.handle = nullptr;
    }
}

} // sf
