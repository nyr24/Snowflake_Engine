#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/image.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;
struct VulkanDevice;

struct VulkanSwapchain {
public:
    static constexpr u8 MAX_FRAMES_IN_FLIGHT{ 2 };
    static constexpr u8 MAX_IMAGE_COUNT{ 10 };

    FixedArray<VkImage, MAX_IMAGE_COUNT>        images;
    FixedArray<VkImageView, MAX_IMAGE_COUNT>    views;
    VkSwapchainKHR                      handle;
    VulkanImage                         depth_image;
    VkSurfaceFormatKHR                  image_format;
    bool                                is_recreating;

public:
    static bool create(
        VulkanDevice& device,
        VkSurfaceKHR surface,
        u16 width,
        u16 height,
        VulkanSwapchain& out_swapchain
    );

    bool recreate(
        VulkanDevice& device,
        VkSurfaceKHR surface,
        u16 width,
        u16 height
    );

    void destroy(const VulkanDevice& device);

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
        VulkanDevice& device,
        VkSurfaceKHR surface,
        u16 width,
        u16 height,
        VulkanSwapchain& out_swapchain
    );
};

} // sf
