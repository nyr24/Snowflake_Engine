#include "sf_vulkan/image.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool VulkanImage::create(
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
    sf_vk_check(vkCreateImage(device.logical_device, &create_info, nullptr, &out_image.handle));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device.logical_device, out_image.handle, &memory_requirements);

    Option<u32> memory_type = device.find_memory_index(memory_requirements.memoryTypeBits, static_cast<VkMemoryPropertyFlagBits>(memory_flags));
    if (memory_type.is_none()) {
        LOG_ERROR("Required memory type not found. Image not valid.");
        return false;
    }

    VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = memory_type.unwrap_copy();
    // TODO: custom allocator
    sf_vk_check(vkAllocateMemory(device.logical_device, &allocate_info, nullptr, &out_image.memory));

    sf_vk_check(vkBindImageMemory(device.logical_device, out_image.handle, out_image.memory, 0));

    if (create_view) {
        out_image.view = nullptr;
        out_image.view = VulkanImage::create_view(out_image.handle, device, format, aspect_flags);
    }

    return true;
}

void VulkanImage::transition_layout(
    VkImage image, const VulkanCommandBuffer& cmd_buffer,
    VkImageLayout old_layout, VkImageLayout new_layout,
    VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
    VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask,
    bool is_depth_image
) {
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = is_depth_image ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount =  1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(cmd_buffer.handle, &dep_info);
}

VkImageView VulkanImage::create_view(
    VkImage image,
    const VulkanDevice& device,
    VkFormat format,
    VkImageAspectFlags aspect_flags
) {
    VkImageView view;
        
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
    };

    create_info.subresourceRange.aspectMask = aspect_flags,
    create_info.subresourceRange.baseMipLevel = 0,
    create_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
    create_info.subresourceRange.baseArrayLayer = 0,
    create_info.subresourceRange.layerCount = 1,

    // TODO: custom allocator
    sf_vk_check(vkCreateImageView(device.logical_device, &create_info, nullptr, &view));
    return view;
}

void VulkanImage::destroy(const VulkanDevice& device) {
    if (view) {
        // TODO: custom allocator
        vkDestroyImageView(device.logical_device, view, nullptr);
        view = nullptr;
    }
    if (memory) {
        // TODO: custom allocator
        vkFreeMemory(device.logical_device, memory, nullptr);
        memory = nullptr;
    }
    if (handle) {
        // TODO: custom allocator
        vkDestroyImage(device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

} // sf
