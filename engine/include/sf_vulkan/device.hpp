#pragma once

#include "sf_vulkan/types.hpp"

namespace sf {
bool device_create(VulkanContext& context);
bool device_destroy(VulkanContext& context);
bool device_select(VulkanContext& context);
} // sf
