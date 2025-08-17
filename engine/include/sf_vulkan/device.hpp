#pragma once

#include "sf_vulkan/types.hpp"
#include "sf_containers/optional.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
bool device_create(VulkanContext& context);
void device_destroy(VulkanContext& context);
bool device_select(VulkanContext& context);
bool device_meet_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties& properties,
    const VkPhysicalDeviceFeatures& features,
    const VulkanPhysicalDeviceRequirements& requirements,
    VulkanDeviceQueueFamilyInfo& out_queue_family_info,
    VulkanSwapchainSupportInfo& out_swapchain_support_info
);
void device_query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo& out_support_info);
bool device_detect_depth_format(VulkanDevice& device);
Option<u32> find_memory_index(VulkanContext& context, u32 type_filter, u32 property_flags);
} // sf
