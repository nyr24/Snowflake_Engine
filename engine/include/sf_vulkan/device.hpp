#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;

static constexpr u8 DEVICE_EXTENSION_CAPACITY{ 10 };

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
    FixedArray<const char*, DEVICE_EXTENSION_CAPACITY>&         device_extension_names;
};

struct VulkanSwapchainSupportInfo {
    DynamicArray<VkSurfaceFormatKHR, ArenaAllocator, false> formats;
    DynamicArray<VkPresentModeKHR, ArenaAllocator, false>   present_modes;
    VkSurfaceCapabilitiesKHR            capabilities;
    u32                                 format_count;
    u32                                 present_mode_count;

    VulkanSwapchainSupportInfo();
    VulkanSwapchainSupportInfo(VulkanSwapchainSupportInfo&& rhs) = default;
    VulkanSwapchainSupportInfo& operator=(VulkanSwapchainSupportInfo&& rhs) = default;
    VulkanSwapchainSupportInfo(const VulkanSwapchainSupportInfo& rhs) = delete;
    VulkanSwapchainSupportInfo& operator=(const VulkanSwapchainSupportInfo& rhs) = delete;
};

struct VulkanDeviceQueueFamilyInfo {
    u8 graphics_family_index;
    u8 graphics_available_queue_count;
    u8 present_family_index;
    u8 present_available_queue_count;
    u8 compute_family_index;
    u8 compute_available_queue_count;
    u8 transfer_family_index;
    u8 transfer_available_queue_count;
};

struct VulkanDevice {
public:
    VkPhysicalDevice                    physical_device;
    VkDevice                            logical_device;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceMemoryProperties    memory_properties;
    VkQueue                             graphics_queue;
    VkQueue                             present_queue;
    VkQueue                             transfer_queue;
    VulkanDeviceQueueFamilyInfo         queue_family_info;
    VulkanSwapchainSupportInfo          swapchain_support_info;
    VkFormat                            depth_format;

public:
    static bool create(VulkanContext& context);
    static bool select(VulkanContext& context, FixedArray<const char*, DEVICE_EXTENSION_CAPACITY>& required_extensions);
    static bool meet_requirements(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties& properties,
        const VkPhysicalDeviceFeatures& features,
        const VulkanPhysicalDeviceRequirements& requirements,
        VulkanDeviceQueueFamilyInfo& out_queue_family_info,
        VulkanSwapchainSupportInfo& out_swapchain_support_info
    );
    static void query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo& out_support_info);
    void destroy(VulkanContext& context);
    bool detect_depth_format();
    Option<u32> find_memory_index(u32 type_filter, VkMemoryPropertyFlagBits property_flags) const;
};

} // sf
