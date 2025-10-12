#include "sf_vulkan/resource.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/utility.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/renderer.hpp"
#include <string_view>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace fs = std::filesystem;

namespace sf {

bool Texture::load(
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    const char* file_name,
    Texture& out_texture)
{
    Texture temp_texture;
    
    if (!load_from_disk(file_name, temp_texture)) {
        return false;
    }

    u32 curr_generation = out_texture.generation;
    out_texture.generation = INVALID_ID;

    if (!temp_texture.upload_to_gpu(device, cmd_buffer)) {
        return false;
    }

    Texture old{ out_texture };
    out_texture = temp_texture;

    old.destroy(device);

    if (curr_generation == INVALID_ID) {
        out_texture.generation = 0;
    } else {
        out_texture.generation = curr_generation + 1;
    }

    return true;
}
    
bool Texture::load_from_disk(const char* file_name, Texture& out_texture) {
#ifdef DEBUG
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#else
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#endif

    // detect format
    std::string_view extension{ extract_extension_from_file_path({ texture_path.c_str() }) };
    out_texture.format = Texture::map_extension_to_format(extension);

    SF_ASSERT_MSG(out_texture.format != ImageFormat::COUNT, "Should be valid image format");

    constexpr u32 REQUIRED_CHANNEL_COUNT{ 4 /* out_texture.format == ImageFormat::PNG ? 4u : 3u */ };

    out_texture.pixels = stbi_load(texture_path.c_str(), reinterpret_cast<i32*>(&out_texture.width),
        reinterpret_cast<i32*>(&out_texture.height), reinterpret_cast<i32*>(out_texture.channel_count), REQUIRED_CHANNEL_COUNT);

    if (!out_texture.pixels) {
        return false;
    }
    if (stbi_failure_reason()) {
        LOG_WARN("Texture load failed for texture {},\n\treason: {}", file_name, stbi_failure_reason());
    }

    out_texture.generation = 0;
    out_texture.size = out_texture.width * out_texture.height * REQUIRED_CHANNEL_COUNT;

    // check for transparency for png images
    if (out_texture.format == ImageFormat::PNG) {
        bool has_transparency{false};

        for (u32 i{0}; i < out_texture.size; i += 4) {
            if (out_texture.pixels[i + 3] < 255) {
                has_transparency = true;
                break;
            }
        }

        out_texture.has_transparency = has_transparency;
    } else {
        out_texture.has_transparency = false;
    }

    return true;
}

// THINK: why texture does not get id from acquire_resources?
bool Texture::upload_to_gpu(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer) {
    SF_ASSERT_MSG(cmd_buffer.state == VulkanCommandBufferState::RECORDING_BEGIN, "Should be in begin state");
    
    VulkanBuffer::create(
        device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
        static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), staging_buffer
    );

    staging_buffer.copy_data(device, pixels, size);

    if (!VulkanImage::create(
        device, image, VK_IMAGE_TYPE_2D,
        width, height, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_IMAGE_ASPECT_COLOR_BIT
    )) {
        return false;
    }

    VulkanImage::transition_layout(
        image.handle, cmd_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT
    );

    cmd_buffer.copy_data_from_buffer_to_image(staging_buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VulkanImage::transition_layout(
        image.handle, cmd_buffer,
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
    sf_vk_check(vkCreateSampler(device.logical_device, &sampler_create_info, nullptr, &sampler));
    generation++;
    
    return true;
}

ImageFormat Texture::map_extension_to_format(std::string_view extension) {
    for (u32 i{0}; i < image_extensions.count(); ++i) {
        if (image_extensions[i] == extension) {
            return static_cast<ImageFormat>(i);
        }
    }

    return ImageFormat::COUNT;
}

void Texture::destroy(const VulkanDevice& device) {
    vkDeviceWaitIdle(device.logical_device);
    
    image.destroy(device);
    staging_buffer.destroy(device);

    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }

    generation = INVALID_ID;

    if (sampler) {
        // TODO: custom allocator
        vkDestroySampler(device.logical_device, sampler, nullptr);
        sampler = nullptr;
    }
}

} // sf
