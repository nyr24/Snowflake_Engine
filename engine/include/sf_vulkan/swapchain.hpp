#pragma once

#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

void swapchain_create(
    VulkanContext& context,
    u32 width,
    u32 height,
    VulkanSwapchain& out_swapchain
);

void swapchain_recreate(
    VulkanContext& context,
    u32 width,
    u32 height,
    VulkanSwapchain& out_swapchain
);

void swapchain_destroy(
    VulkanContext& context,
    VulkanSwapchain& swapchain
);

bool swapchain_acquire_next_image_index(
    VulkanContext& context,
    VulkanSwapchain& swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32& out_image_index
);

void swapchain_present(
    VulkanContext& context,
    VulkanSwapchain& swapchain,
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    u32 present_image_index
);

} // sf
