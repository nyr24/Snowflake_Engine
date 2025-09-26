#include "sf_vulkan/resource.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/renderer.hpp"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace fs = std::filesystem;

namespace sf {

// TODO: 
// All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
// For practical applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput,
// especially the transitions and copy in the createTextureImage function. Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into,
// and add a flushSetupCommands to execute the commands that have been recorded so far. It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.

bool Texture::create(const VulkanContext& context, const char* file_name, bool has_transparency, Texture& out_texture) {
#ifdef DEBUG
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#else
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#endif

    out_texture.pixels = stbi_load(texture_path.c_str(), reinterpret_cast<i32*>(&out_texture.width), reinterpret_cast<i32*>(&out_texture.height), reinterpret_cast<i32*>(out_texture.channel_count), STBI_rgb_alpha);
    if (!out_texture.pixels) {
        return false;
    }

    u32 image_size = out_texture.width * out_texture.height * 4;

    VulkanBuffer staging_buffer;
    VulkanBuffer::create(
        context.device, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), staging_buffer
    );

    if (!VulkanImage::create(
        context.device, out_texture.image, VK_IMAGE_TYPE_2D,
        out_texture.width, out_texture.height, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_IMAGE_ASPECT_COLOR_BIT
    )) {
        return false;
    }

    VulkanCommandBuffer cmd_buffer;
    cmd_buffer.allocate_and_begin_single_use(context, context.graphics_command_pool.handle, cmd_buffer);

    VulkanImage::transition_layout(
        out_texture.image.handle, cmd_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT
    );

    cmd_buffer.copy_data_from_buffer_to_image(staging_buffer.handle, out_texture.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VulkanImage::transition_layout(
        out_texture.image.handle, cmd_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT
    );

    VkSamplerCreateInfo sampler_create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,  
        .minFilter = VK_FILTER_LINEAR,  
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,    
        .maxAnisotropy = 16,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    // TODO: custom allocator
    sf_vk_check(vkCreateSampler(context.device.logical_device, &sampler_create_info, nullptr, &out_texture.sampler));

    out_texture.has_transparency = has_transparency;
    out_texture.generation++;

    staging_buffer.destroy(context.device);
    cmd_buffer.end_single_use(context, context.graphics_command_pool.handle);

    return true;
}

void Texture::destroy(const VulkanDevice& device) {
    image.destroy(device);
    // TODO: custom allocator
    vkDestroySampler(device.logical_device, sampler, nullptr);
}

} // sf
