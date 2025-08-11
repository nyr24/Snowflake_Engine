#pragma once

// Shared types between vulkan subsystem

#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_vulkan/allocator.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct PlatformState;

// device
struct VulkanDevice {
    VkPhysicalDevice                    physical_device;
    VkDevice                            logical_device;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceMemoryProperties    memory_properties;
    u32                                 graphics_queue_index;
    u32                                 present_queue_index;
    u32                                 transfer_queue_index;
    u32                                 compute_queue_index;
};

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

struct VulkanDeviceQueueFamilyInfo {
    u32 graphics_family_index;
    u32 present_family_index;
    u32 compute_family_index;
    u32 transfer_family_index;
};

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR            capabilities;
    DynamicArray<VkSurfaceFormatKHR>    formats;
    DynamicArray<VkPresentModeKHR>      present_modes;
    u32                                 format_count;
    u32                                 present_mode_count;
};

// context
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

// renderer
struct RenderPacket {
    f64 delta_time;
};

struct VulkanRenderer {
    PlatformState*  platform_state;
    u64             frame_count;

public:
    VulkanRenderer() = default;
};

// shared functions
void sf_vk_check(VkResult vk_result);
u32 sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData
);

} // sf
