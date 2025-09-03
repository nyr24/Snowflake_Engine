#pragma once

// Shared types between vulkan subsystem

#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/swapchain.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/synch.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct PlatformState;
struct VulkanContext;

// struct VulkanAllocator {
//     VkAllocationCallbacks callbacks;
// };

using VulkanAllocator = VkAllocationCallbacks;

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
    VulkanBuffer                        vertex_buffer;
    FixedArray<VulkanCommandBuffer, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>  graphics_command_buffers;
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>      image_available_semaphores;
    FixedArray<VulkanSemaphore, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>      render_finished_semaphores;
    FixedArray<VulkanFence, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT>          draw_fences;
    u32                                 image_index;
    u32                                 curr_frame;
    u32                                 framebuffer_size_generation;
    u32                                 framebuffer_last_size_generation;
    u16                                 framebuffer_width;
    u16                                 framebuffer_height;
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
