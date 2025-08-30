#pragma once

#include <string_view>
#include <vulkan/vulkan_core.h>

namespace sf {
  constexpr std::string_view vk_result_string(VkResult result, bool get_extended);
} // sf
