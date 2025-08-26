#pragma once

// Shared types between vulkan subsystem

#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/synch.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct PlatformState;
struct VulkanContext;

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

struct VulkanSwapchainSupportInfo {
    VkSurfaceCapabilitiesKHR            capabilities;
    DynamicArray<VkSurfaceFormatKHR>    formats;
    DynamicArray<VkPresentModeKHR>      present_modes;
    u32                                 format_count;
    u32                                 present_mode_count;
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

// struct VulkanAllocator {
//     VkAllocationCallbacks callbacks;
// };
using VulkanAllocator = VkAllocationCallbacks;

struct VulkanPipeline {
    VkPipeline          handle;
    VkPipelineLayout    layout;
};

// context
struct VulkanContext {
    VkInstance                          instance;
    VulkanAllocator                     allocator;
    VulkanDevice                        device;
    VkSurfaceKHR                        surface;
#ifdef SF_DEBUG
    VkDebugUtilsMessengerEXT            debug_messenger;
#endif
    VulkanSwapchain                     swapchain;
    VulkanPipeline                      pipeline;
    VulkanCommandPool                   graphics_command_pool;
    FixedArray<VulkanCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>  graphics_command_buffers;
    // synch primitives
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>      image_available_semaphores;
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>      render_finished_semaphores;
    FixedArray<VulkanFence, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>          draw_fences;
    u32                                 image_index;
    u32                                 curr_frame;
    u32                                 framebuffer_width;
    u32                                 framebuffer_height;
    bool                                recreating_swapchain;
public:
    VulkanContext();
    ~VulkanContext();

    VkViewport get_viewport() const;
    VkRect2D get_scissors() const;
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
