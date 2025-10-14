#include "sf_vulkan/texture.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_containers/hashmap.hpp"
#include <string_view>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace fs = std::filesystem;

namespace sf {

static u32 get_new_texture_id();

Texture::Texture()
    : id{ INVALID_ID }
    , generation{ INVALID_ID }
    , ref_count{0}
    , state{ TextureState::NOT_LOADED }
{}

bool Texture::load(
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    Texture& out_texture,
    TextureInputConfig config
)
{
    out_texture.auto_release = config.auto_release;

    if (out_texture.state == TextureState::NOT_LOADED) {
        if (!out_texture.load_from_disk(config.file_name)) {
            return false;
        }
        if (!out_texture.upload_to_gpu(device, cmd_buffer)) {
            return false;
        }
    } else if (out_texture.state == TextureState::LOADED_FROM_DISK) {
        if (!out_texture.upload_to_gpu(device, cmd_buffer)) {
            return false;
        }
    }

    if (out_texture.generation == INVALID_ID) {
        out_texture.generation = 0;
    } else {
        out_texture.generation++;
    }

    return true;
}
    
bool Texture::load_from_disk(std::string_view file_name) {
#ifdef DEBUG
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#else
    fs::path texture_path = fs::current_path() / "build" / "debug/engine/textures/" / file_name;
#endif

    // detect format
    std::string_view extension{ extract_extension_from_file_path({ texture_path.c_str() }) };
    Result<ImageFormat> maybe_format = Texture::map_extension_to_format(extension);

    if (maybe_format.is_err()) {
        LOG_ERROR("Should be valid image format");
        return false;
    }

    format = maybe_format.unwrap_copy();
    constexpr u32 REQUIRED_CHANNEL_COUNT{ 4 /* format == ImageFormat::PNG ? 4u : 3u */ };

    pixels = stbi_load(texture_path.c_str(), reinterpret_cast<i32*>(&width),
        reinterpret_cast<i32*>(&height), reinterpret_cast<i32*>(&channel_count), REQUIRED_CHANNEL_COUNT);

    if (!pixels) {
        return false;
    }
    if (stbi_failure_reason()) {
        LOG_WARN("Load warning/error for texture {},\n\tmessage: {}", file_name, stbi_failure_reason());
    }

    id = get_new_texture_id();
    generation = 0;
    size = width * height * REQUIRED_CHANNEL_COUNT;

    // check for transparency for png images
    if (format == ImageFormat::PNG) {
        bool has_transparency{false};

        for (u32 i{0}; i < size; i += 4) {
            if (pixels[i + 3] < 255) {
                has_transparency = true;
                break;
            }
        }

        has_transparency = has_transparency;
    } else {
        has_transparency = false;
    }

    state = TextureState::LOADED_FROM_DISK;

    return true;
}

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

    state = TextureState::UPLOADED_TO_GPU;
    
    return true;
}

Result<ImageFormat> Texture::map_extension_to_format(std::string_view extension) {
    for (u32 i{0}; i < image_extensions.count(); ++i) {
        if (image_extensions[i] == extension) {
            return {static_cast<ImageFormat>(i)};
        }
    }

    return {ResultError::VALUE};
}

void Texture::destroy(const VulkanDevice& device) {
    vkDeviceWaitIdle(device.logical_device);
    
    image.destroy(device);
    staging_buffer.destroy(device);

    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }

    ref_count = 0;
    state = TextureState::NOT_LOADED;
    id = INVALID_ID;
    generation = INVALID_ID;

    if (sampler) {
        // TODO: custom allocator
        vkDestroySampler(device.logical_device, sampler, nullptr);
        sampler = nullptr;
    }
}

// Texture System State

struct TextureSystemState {
    static constexpr u32 DEFAULT_PREALLOC_TEXTURES{20};

    HashMap<std::string_view, Texture> textures;
    u32 id_counter;

    TextureSystemState()
        : textures(DEFAULT_PREALLOC_TEXTURES)
    {}
};

static TextureSystemState state;

static void load_texture_and_put_into_table(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, TextureInputConfig config) {
    Texture new_texture;
    if (!Texture::load(device, cmd_buffer, new_texture, config)) {
        LOG_ERROR("Texture with name {} fails to load", config.file_name);
        return;
    }
    state.textures.put(config.file_name, std::move(new_texture));
}

void texture_system_create_textures(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<const TextureInputConfig> texture_configs) {
    stbi_set_flip_vertically_on_load(true);
    for (const auto& config : texture_configs) {
        Option<Texture*> maybe_texture = state.textures.get(config.file_name);
        if (maybe_texture.is_some()) {
            LOG_WARN("Texture with name {} already exists", config.file_name);
            continue;
        }
        load_texture_and_put_into_table(device, cmd_buffer, config);
    }
}

// loads into table if not loaded, otherwise just returns a pointer
Texture* texture_system_get_texture(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, const TextureInputConfig config) {
    Option<Texture*> maybe_texture = state.textures.get(config.file_name);
    if (maybe_texture.is_none()) {
        load_texture_and_put_into_table(device, cmd_buffer, config);
        Option<Texture*> newly_created_texture = state.textures.get(config.file_name);
        return newly_created_texture.unwrap_copy();
    }

    Texture* texture{ maybe_texture.unwrap_copy() };
    texture->ref_count++;
    return texture;
}

// only frees the texture if ref_count was 1
bool texture_system_free_texture(const VulkanDevice& device, std::string_view name) {
    Option<Texture*> maybe_texture = state.textures.get(name);
    if (maybe_texture.is_none()) {
        LOG_WARN("Trying to delete texture that does not exist: {}", name);
        return false;
    }

    Texture* texture{ maybe_texture.unwrap_copy() };
    if (texture->ref_count == 1) {
        texture->destroy(device);        
        state.textures.remove(name);
        return true;
    }

    texture->ref_count--;
    return false;
}

static u32 get_new_texture_id() {
    return state.id_counter++;
}

} // sf
