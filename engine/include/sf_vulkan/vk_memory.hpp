#pragma once

#include "sf_vulkan/device.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanMemory {
    VkDeviceMemory      handle;
    const VulkanDevice& device;
};

} // sf
