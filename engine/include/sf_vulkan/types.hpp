#pragma once

// Shared types between vulkan subsystem

#include "sf_core/types.hpp"
#include "sf_vulkan/allocator.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanDevice {
    VkPhysicalDevice    physical_device;
    VkDevice            logical_device;
};

struct VulkanContext {
    VkInstance                  instance;
    VulkanAllocator             allocator;
    VulkanDevice                device;
    VkSurfaceKHR                surface;

#ifdef SF_DEBUG
    VkDebugUtilsMessengerEXT    debug_messenger;
#endif
public:
    ~VulkanContext();
};

void sf_vk_check(VkResult vk_result);
u32 sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData
);

} // sf
