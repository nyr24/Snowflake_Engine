#pragma once

// Shared types between vulkan subsystem

#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct PlatformState;

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

struct VulkanImage {
    VkImage         handle;
    VkDeviceMemory  memory;
    VkImageView     view;
    u32             width;
    u32             height;
};

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR            capabilities;
    DynamicArray<VkSurfaceFormatKHR>    formats;
    DynamicArray<VkPresentModeKHR>      present_modes;
    u32                                 format_count;
    u32                                 present_mode_count;
};

struct VulkanSwapchain {
    static constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    VkSwapchainKHR                      handle;
    DynamicArray<VkImage>               images;
    DynamicArray<VkImageView>           views;
    VulkanImage                         depth_attachment;
    VkSurfaceFormatKHR                  image_format;
};

// device
struct VulkanDevice {
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

// struct VulkanAllocator {
//     VkAllocationCallbacks callbacks;
// };
using VulkanAllocator = VkAllocationCallbacks;

// context
struct VulkanContext {
    VkInstance                  instance;
    VulkanAllocator             allocator;
    VulkanDevice                device;
    VkSurfaceKHR                surface;
#ifdef SF_DEBUG
    VkDebugUtilsMessengerEXT    debug_messenger;
#endif
    i32                         (*find_memory_index)(u32 type_filter, u32 property_flags);
    VulkanSwapchain             swapchain;
    u32                         image_index;
    u32                         curr_frame;
    u32                         framebuffer_width;
    u32                         framebuffer_height;
    bool                        recreating_swapchain;
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
