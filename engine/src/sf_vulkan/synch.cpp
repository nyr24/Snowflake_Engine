#include "sf_vulkan/synch.hpp"
#include "sf_core/logger.hpp"
#include "sf_platform/platform.hpp"
#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

VulkanSemaphore VulkanSemaphore::create(const VulkanContext& context) {
    VulkanSemaphore semaphore{ .handle = VK_NULL_HANDLE };
    
    VkSemaphoreCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateSemaphore(context.device.logical_device, &create_info, nullptr, &semaphore.handle));

    return semaphore;
}

void VulkanSemaphore::destroy(const VulkanContext& context) {
    if (handle) {
        // TODO: custom allocator
        vkDestroySemaphore(context.device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

VulkanFence VulkanFence::create(const VulkanContext& context, bool create_singaled) {
    VulkanFence fence{ .handle = VK_NULL_HANDLE, .is_signaled = create_singaled };

    VkFenceCreateInfo create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

    if (create_singaled) {
        create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    // TODO: custom allocator
    sf_vk_check(vkCreateFence(context.device.logical_device, &create_info, nullptr, &fence.handle));

    return fence;
}

bool VulkanFence::wait(const VulkanContext& context) {
    if (is_signaled) {
        return false;
    }

    VkResult result = VK_TIMEOUT;

    while (true) {
        result = vkWaitForFences(context.device.logical_device, 1, &handle, VK_TRUE, UINT64_MAX);
        if (result != VK_TIMEOUT) {
            break;
        }
    }
    
    if (result == VK_SUCCESS) {
        is_signaled = true;
        return true;
    } else {
        LOG_FATAL("Fence wait error occurred");
        return false;
    }
}

void VulkanFence::reset(const VulkanContext& context) {
    if (!is_signaled) {
        return;
    }
    
    sf_vk_check(vkResetFences(context.device.logical_device, 1, &handle));
    is_signaled = false;
}

void VulkanFence::destroy(const VulkanContext& context) {
    if (handle) {
        // TODO: custom allocator
        vkDestroyFence(context.device.logical_device, handle, nullptr);
        handle = nullptr;
    }
}

} // sf