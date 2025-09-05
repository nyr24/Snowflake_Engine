#pragma once

#include <filesystem>
#include "sf_containers/result.hpp"
#include "sf_vulkan/renderer.hpp"

namespace sf {

Result<VkShaderModule> create_shader_module(VulkanContext& context, std::filesystem::path shader_file_path);

} // sf
