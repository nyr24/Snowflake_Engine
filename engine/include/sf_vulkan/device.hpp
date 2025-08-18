#pragma once

#include "sf_vulkan/types.hpp"
#include "sf_containers/optional.hpp"
#include <vulkan/vulkan_core.h>
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"

namespace sf {

enum VulkanPhysicalDeviceRequirementsBits : u16 {
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_GRAPHICS = 1 << 0x0000,
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_PRESENT = 1 << 0x0001,
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_COMPUTE = 1 << 0x0002,
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_TRANSFER = 1 << 0x0003,
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_SAMPLER_ANISOTROPY = 1 << 0x0004,
    VULKAN_PHYSICAL_DEVICE_REQUIREMENT_DISCRETE_GPU = 1 << 0x0005,
};

using VulkanPhysicalDeviceRequirementsFlags = u32;

bool requirements_has_graphics(VulkanPhysicalDeviceRequirementsFlags requirements);
bool requirements_has_present(VulkanPhysicalDeviceRequirementsFlags requirements);
bool requirements_has_compute(VulkanPhysicalDeviceRequirementsFlags requirements);
bool requirements_has_transfer(VulkanPhysicalDeviceRequirementsFlags requirements);
bool requirements_has_sampler_anisotropy(VulkanPhysicalDeviceRequirementsFlags requirements);
bool requirements_has_discrete(VulkanPhysicalDeviceRequirementsFlags requirements);

struct VulkanPhysicalDeviceRequirements {
    static constexpr u32 MAX_DEVICE_EXTENSION_NAMES{ 10 };

    VulkanPhysicalDeviceRequirementsFlags                       flags;
    Option<FixedArray<const char*, MAX_DEVICE_EXTENSION_NAMES>> device_extension_names;
};

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
