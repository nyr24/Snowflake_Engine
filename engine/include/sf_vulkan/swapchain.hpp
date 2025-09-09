#pragma once

#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_vulkan/image.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;

struct VulkanSwapchain {
public:
    static constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    VkSwapchainKHR                      handle;
    DynamicArray<VkImage>               images;
    DynamicArray<VkImageView>           views;
    VulkanImage                         depth_attachment;
    VkSurfaceFormatKHR                  image_format;

public:
    static bool create(
        VulkanContext& context,
        u16 width,
        u16 height,
        VulkanSwapchain& out_swapchain
    );

    bool recreate(
        VulkanContext& context,
        u16 width,
        u16 height
    );

    void destroy(const VulkanContext& context);

    bool acquire_next_image_index(
        VulkanContext& context,
        u64 timeout_ns,
        VkSemaphore image_available_semaphore,
        VkFence fence,
        u32& out_image_index
    );
    
    void present(
        VulkanContext& context,
        VkQueue present_queue,
        VkSemaphore render_complete_semaphore,
        u32& present_image_index
    );

private:
    static bool create_inner(
        VulkanContext& context,
        u16 width,
        u16 height,
        VulkanSwapchain& out_swapchain
    );
};

} // sf
