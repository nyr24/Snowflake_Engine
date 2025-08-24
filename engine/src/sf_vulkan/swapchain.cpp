#include "sf_vulkan/swapchain.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

bool VulkanSwapchain::create(
    VulkanContext& context,
    u32 width,
    u32 height,
    VulkanSwapchain& out_swapchain
) {
    return create_inner(context, width, height, out_swapchain);
}

void VulkanSwapchain::recreate(
    VulkanContext& context,
    u32 width,
    u32 height
) {
    destroy(context);
    create(context, width, height, *this);
}

bool VulkanSwapchain::acquire_next_image_index(
    VulkanContext& context,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32& out_image_index
) {
    VkResult result = vkAcquireNextImageKHR(context.device.logical_device, handle, timeout_ns, image_available_semaphore, fence, &out_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate(context, context.framebuffer_width, context.framebuffer_height);
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_FATAL("Failed to acquire swapchain image!");
        return false;
    }

    return true;
}

void VulkanSwapchain::present(
    VulkanContext& context,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    u32 present_image_index
) {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_complete_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &handle,
        .pImageIndices = &present_image_index,
        .pResults = nullptr,
    };

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(context, context.framebuffer_width, context.framebuffer_height);
    } else if (result != VK_SUCCESS) {
        LOG_FATAL("Failed to present swapchain image");
    }
}

bool VulkanSwapchain::create_inner(
    VulkanContext& context,
    u32 width,
    u32 height,
    VulkanSwapchain& swapchain
) {
    VkExtent2D swapchain_extent{width, height};

    // Choose a swap surface format.
    bool found = false;
    for (u32 i = 0; i < context.device.swapchain_support_info.format_count; ++i) {
        VkSurfaceFormatKHR format = context.device.swapchain_support_info.formats[i];
        // Preferred formats
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchain.image_format = format;
            found = true;
            break;
        }
    }

    if (!found) {
        swapchain.image_format = context.device.swapchain_support_info.formats[0];
    }

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < context.device.swapchain_support_info.present_mode_count; ++i) {
        VkPresentModeKHR mode = context.device.swapchain_support_info.present_modes[i];
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = mode;
            break;
        }
    }

     // Requery swapchain support.
    device_query_swapchain_support(
        context.device.physical_device,
        context.surface,
        context.device.swapchain_support_info
    );

    // Swapchain extent
    if (context.device.swapchain_support_info.capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent = context.device.swapchain_support_info.capabilities.currentExtent;
    }

    // Clamp to the value allowed by the GPU.
    VkExtent2D min = context.device.swapchain_support_info.capabilities.minImageExtent;
    VkExtent2D max = context.device.swapchain_support_info.capabilities.maxImageExtent;
    swapchain_extent.width = sf_clamp<u32>(swapchain_extent.width, min.width, max.width);
    swapchain_extent.height = sf_clamp<u32>(swapchain_extent.height, min.height, max.height);

    u32 image_count = context.device.swapchain_support_info.capabilities.minImageCount + 1;
    if (context.device.swapchain_support_info.capabilities.maxImageCount > 0 && image_count > context.device.swapchain_support_info.capabilities.maxImageCount) {
        image_count = context.device.swapchain_support_info.capabilities.maxImageCount;
    }

    // Swapchain create info
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.surface,
        .minImageCount = image_count,
        .imageFormat = swapchain.image_format.format,
        .imageColorSpace = swapchain.image_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    };

    // Setup the queue family indices
    if (context.device.queue_family_info.graphics_family_index != context.device.queue_family_info.present_family_index) {
        u32 queueFamilyIndices[] = {
            static_cast<u32>(context.device.queue_family_info.graphics_family_index),
            static_cast<u32>(context.device.queue_family_info.present_family_index)
        };
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices = 0;
    }

    swapchain_create_info.preTransform = context.device.swapchain_support_info.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = 0;

    // TODO: custom allocator
    // sf_vk_check(vkCreateSwapchainKHR(context.device.logical_device, &swapchain_create_info, &context.allocator, &swapchain.handle));
    sf_vk_check(vkCreateSwapchainKHR(context.device.logical_device, &swapchain_create_info, nullptr, &swapchain.handle));

    // Start with a zero frame index.
    context.curr_frame = 0;

    // Images
    u32 swapchain_image_count = 0;
    sf_vk_check(vkGetSwapchainImagesKHR(context.device.logical_device, swapchain.handle, &swapchain_image_count, 0));
    if (swapchain.images.is_empty()) {
        swapchain.images.resize(swapchain_image_count);
    }
    if (swapchain.views.is_empty()) {
        swapchain.views.resize(swapchain_image_count);
    }

    // resize command buffers by image count amount
    context.graphics_command_buffers.resize(image_count);

    sf_vk_check(vkGetSwapchainImagesKHR(
        context.device.logical_device,
        swapchain.handle,
        &swapchain_image_count,
        swapchain.images.data()
    ));

    // Views
    for (u32 i = 0; i < swapchain_image_count; ++i) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain.images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain.image_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        // TODO: custom allocator
        // sf_vk_check(vkCreateImageView(context.device.logical_device, &view_info, &context.allocator, &swapchain.views[i]));
        sf_vk_check(vkCreateImageView(context.device.logical_device, &view_info, nullptr, &swapchain.views[i]));
    }

    if (!device_detect_depth_format(context.device)) {
        context.device.depth_format = VK_FORMAT_UNDEFINED;
        LOG_FATAL("Failed to find a supported format");
    }

    // Create depth image and its view.
    bool create_result = VulkanImage::create(
        context,
        swapchain.depth_attachment,
        VK_IMAGE_TYPE_2D,
        swapchain_extent.width,
        swapchain_extent.height,
        context.device.depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    if (!create_result) {
        LOG_FATAL("Error when creating swapchain depth attachment image");
        return false;
    }

    LOG_INFO("Swapchain created successfully.");
    return true;
}

void VulkanSwapchain::destroy(const VulkanContext& context) {
    depth_attachment.destroy(context);

    if (context.device.logical_device) {
        for (u32 i{0}; i < views.count(); ++i) {
            // TODO: custom allocator
            // vkDestroyImageView(context.device.logical_device, swapchain.views[i], &context.allocator);
            vkDestroyImageView(context.device.logical_device, views[i], nullptr);
        }

        images.clear();
        views.clear();
    }

    if (context.device.logical_device && handle) {
        // TODO: custom allocator
        // vkDestroySwapchainKHR(context.device.logical_device, swapchain.handle, &context.allocator);
        vkDestroySwapchainKHR(context.device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

} // sf
