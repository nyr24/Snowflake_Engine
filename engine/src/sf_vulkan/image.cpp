#include "sf_vulkan/image.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool VulkanImage::create(
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
    VkImageLayout image_init_layout
) {
    out_image.width = width;
    out_image.height = height;

    VkImageCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .mipLevels = 4,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = image_init_layout,
    };

    create_info.extent.width = width,
    create_info.extent.height = height,
    create_info.extent.depth = 1,

    // TODO: custom allocator
    // sf_vk_check(vkCreateImage(context.device.logical_device, &create_info, &context.allocator, &out_image.handle));
    sf_vk_check(vkCreateImage(context.device.logical_device, &create_info, nullptr, &out_image.handle));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context.device.logical_device, out_image.handle, &memory_requirements);

    Option<u32> memory_type = context.device.find_memory_index(memory_requirements.memoryTypeBits, static_cast<VkMemoryPropertyFlagBits>(memory_flags));
    if (memory_type.is_none()) {
        LOG_ERROR("Required memory type not found. Image not valid.");
        return false;
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
        out_image.create_view(context, format, aspect_flags);
    }

    return true;
}

void VulkanImage::create_view(
    const VulkanContext& context,
    VkFormat format,
    VkImageAspectFlags aspect_flags
) {
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
    };

    create_info.subresourceRange.aspectMask = aspect_flags,
    create_info.subresourceRange.baseMipLevel = 0,
    create_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
    create_info.subresourceRange.baseArrayLayer = 0,
    create_info.subresourceRange.layerCount = 1,

    // TODO: custom allocator
    // sf_vk_check(vkCreateImageView(context.device.logical_device, &create_info, &context.allocator, &image.view));
    sf_vk_check(vkCreateImageView(context.device.logical_device, &create_info, nullptr, &view));
}

void VulkanImage::destroy(const VulkanContext& context) {
    if (view) {
        // TODO: custom allocator
        // vkDestroyImageView(context.device.logical_device, image.view, &context.allocator);
        vkDestroyImageView(context.device.logical_device, view, nullptr);
        view = nullptr;
    }
    if (memory) {
        // TODO: custom allocator
        // vkFreeMemory(context.device.logical_device, image.memory, &context.allocator);
        vkFreeMemory(context.device.logical_device, memory, nullptr);
        memory = nullptr;
    }
    if (handle) {
        // TODO: custom allocator
        // vkDestroyImage(context.device.logical_device, image.handle, &context.allocator);
        vkDestroyImage(context.device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

} // sf
