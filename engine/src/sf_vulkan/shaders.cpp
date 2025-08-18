#include "sf_vulkan/shaders.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/io.hpp"
#include "sf_vulkan/types.hpp"
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace sf {

Result<VkShaderModule> create_shader_module(VulkanContext& context, std::filesystem::path shader_file_path) {
    Result<DynamicArray<char>> shader_file_contents = read_file(shader_file_path);

    if (shader_file_contents.is_err()) {
        return {ResultError::VALUE};
    }

    DynamicArray<char>& shader_file_contents_unwrapped{ shader_file_contents.unwrap() };

    VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_file_contents,
        .pCode = reinterpret_cast<u32*>(shader_file_contents_unwrapped.data()),
    };

    VkShaderModule shader_handle;

    // TODO: custom allocator
    sf_vk_check(vkCreateShaderModule(context.device.logical_device, &create_info, nullptr, &shader_handle));
    return {shader_handle};
}

} // sf
