#pragma once

#include <vulkan/vulkan_core.h>

namespace sf {
struct VulkanContext;

struct VulkanSemaphore {
    VkSemaphore handle;

    static VulkanSemaphore create(const VulkanContext& context);
    void destroy(const VulkanContext& context);
};

struct VulkanFence {
    VkFence handle;
    bool    is_signaled;

    static VulkanFence create(const VulkanContext& context, bool create_singaled);
    bool wait(const VulkanContext& context);
    void reset(const VulkanContext& context);
    void destroy(const VulkanContext& context);
};

} // sf
