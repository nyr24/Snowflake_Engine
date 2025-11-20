#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/io.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/renderer.hpp"
#include <string_view>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <vulkan/vulkan_core.h>

namespace sf {

static TextureSystem* state_ptr;

void Texture::create_empty(u32 id, Texture& out_texture) {
    out_texture.id = id;
    out_texture.generation = INVALID_ID;
    out_texture.state = TextureState::NOT_LOADED;
}
    
bool Texture::load(
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    TextureInputConfig config,
    StackAllocator& alloc,
    Texture& out_texture
)
{
    if (out_texture.state == TextureState::NOT_LOADED) {
        if (!out_texture.load_from_disk(config.file_name, alloc)) {
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

bool Texture::load_from_disk(std::string_view file_name, StackAllocator& alloc) {
#ifdef SF_DEBUG
    std::string_view init_path = "build/debug/engine/assets/textures/";
#else
    std::string_view init_path = "build/release/engine/assets/textures/";
#endif
    StringBacked<StackAllocator> texture_path(init_path.size() + file_name.size() + 1, &alloc);
    texture_path.append_sv(init_path);
    texture_path.append_sv(file_name);
    texture_path.append('\0');

    // detect format
    std::string_view extension{ extract_extension_from_file_name(texture_path.to_string_view()) };
    Result<ImageFormat> maybe_format = Texture::map_extension_to_format(extension);

    texture_path.append('\0');

    if (maybe_format.is_err()) {
        LOG_ERROR("Should be valid image format");
        return false;
    }

    format = maybe_format.unwrap_copy();
    constexpr u32 REQUIRED_CHANNEL_COUNT{ 4 };

    pixels = stbi_load(texture_path.data(), reinterpret_cast<i32*>(&width),
        reinterpret_cast<i32*>(&height), reinterpret_cast<i32*>(&channel_count), REQUIRED_CHANNEL_COUNT);

    if (!pixels) {
        if (stbi_failure_reason()) {
            LOG_WARN("Load warning/error for texture {},\n\tmessage: {}", file_name, stbi_failure_reason());
            stbi__err(0, 0);
        }
        return false;
    }

    generation = 0;
    size = width * height * REQUIRED_CHANNEL_COUNT;

    // check for transparency for png images
    if (format == ImageFormat::PNG) {
        bool texture_has_transparency{false};

        for (u32 i{0}; i < size; i += 4) {
            if (pixels[i + 3] < 255) {
                texture_has_transparency = true;
                break;
            }
        }

        has_transparency = texture_has_transparency;
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
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        false
    );

    cmd_buffer.copy_data_from_buffer_to_image(staging_buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VulkanImage::transition_layout(
        image.handle, cmd_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        false
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
    if (device.logical_device) {
        vkDeviceWaitIdle(device.logical_device);
        image.destroy(device);
        staging_buffer.destroy(device);

        if (sampler) {
            // TODO: custom allocator
            vkDestroySampler(device.logical_device, sampler, nullptr);
            sampler = nullptr;
        }
    }

    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }

    state = TextureState::NOT_LOADED;
    id = INVALID_ID;
    generation = INVALID_ID;
}

// Texture System State

void TextureSystem::create(ArenaAllocator& allocator, const VulkanDevice& device, TextureSystem& out_system) {
    state_ptr = &out_system;
    out_system.textures.set_allocator(&allocator);
    out_system.textures.resize(MAX_TEXTURE_AMOUNT);
    out_system.texture_lookup_table.set_allocator(&allocator);
    out_system.texture_lookup_table.reserve(MAX_TEXTURE_AMOUNT);
    out_system.device = &device;

    auto& textures = out_system.textures;
    u32 cap = textures.capacity();

    for (u32 i{0}; i < cap; ++i) {
        Texture::create_empty(i, textures[i]);
    }
    for (auto& t : out_system.texture_lookup_table) {
        t.value.handle = INVALID_ID;
    }

    stbi_set_flip_vertically_on_load(true);
}

TextureSystem::~TextureSystem()
{
    if (device) {
        for (auto& t : textures) {
            t.destroy(*device);
        }
    }
}

Texture& TextureSystem::get_empty_slot() {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");
    
    auto& textures{ state_ptr->textures };
    u32 count{ textures.count() };

    for (u32 i{0}; i < count; ++i) {
        if (textures[i].generation == INVALID_ID) {
            textures[i].generation = 0;
            return textures[i];
        }
    }

    // empty slot not found, do resizing
    u32 new_id = textures.count();
    textures.resize_exponent(textures.count() + 1);
    Texture::create_empty(new_id, textures[new_id]);

    count = textures.count();
    
    for (u32 i{new_id + 1}; i < count; ++i) {
        Texture::create_empty(i, textures[i]);
    }

    return textures[new_id];
}

static bool load_texture_and_put_into_table(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, TextureInputConfig config, StackAllocator& alloc) {
    Texture& new_texture = TextureSystem::get_empty_slot();
    if (!Texture::load(device, cmd_buffer, config, alloc, new_texture)) {
        LOG_ERROR("Texture with name {} fails to load", config.file_name);
        return false;
    }

    std::string_view texture_name{ strip_extension_from_file_name(config.file_name) };

    TextureRef texture_ref{
        .handle = new_texture.id,
        .ref_count = 0,
        .auto_release = config.auto_release,
    };
    
    state_ptr->texture_lookup_table.put(texture_name, texture_ref);
    return true;
}

Texture* TextureSystem::get_or_load_texture(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, const TextureInputConfig config, StackAllocator& alloc) {
    std::string_view texture_name{ strip_extension_from_file_name(config.file_name) };
    Option<TextureRef*> maybe_texture = state_ptr->texture_lookup_table.get(texture_name);

    if (maybe_texture.is_none()) {
        if (!load_texture_and_put_into_table(device, cmd_buffer, config, alloc)) {
            return nullptr;
        }
        TextureRef* newly_created_texture_ref = state_ptr->texture_lookup_table.get(texture_name).unwrap_copy();
        newly_created_texture_ref->ref_count++;
        return &state_ptr->textures[newly_created_texture_ref->handle];
    }

    TextureRef* texture_ref{ maybe_texture.unwrap_copy() };
    texture_ref->ref_count++;
    return &state_ptr->textures[texture_ref->handle];
}

void TextureSystem::get_or_load_textures_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc,  std::span<TextureInputConfig> configs, std::span<Texture*> out_textures) {
#ifdef SF_DEBUG
    if (configs.size() > out_textures.size()) {
        LOG_WARN("Texture configs array has more items than can fit into out_textures array: {} - {}", configs.size(), out_textures.size());
    }
#endif

    const u32 min_len = std::min(configs.size(), out_textures.size());
    
    for (u32 i{0}; i < min_len; ++i) {
        out_textures[i] = TextureSystem::get_or_load_texture(device, cmd_buffer, configs[i], alloc);
    }
}

Texture* TextureSystem::get_texture(std::string_view file_name) {
    std::string_view texture_name{ strip_extension_from_file_name(file_name) };
    Option<TextureRef*> maybe_texture = state_ptr->texture_lookup_table.get(texture_name);
    if (maybe_texture.is_none()) {
        LOG_ERROR("TextureSystem::get_texture: don't have a texture with a name: {}", file_name);
        return nullptr;
    }
    
    TextureRef* texture_ref{ maybe_texture.unwrap_copy() };
    texture_ref->ref_count++;
    return &state_ptr->textures[texture_ref->handle];
}

// only frees the texture if ref_count was 1
void TextureSystem::free_texture(const VulkanDevice& device, std::string_view file_name) {
    std::string_view texture_name{ strip_extension_from_file_name(file_name) };
    Option<TextureRef*> maybe_texture = state_ptr->texture_lookup_table.get(texture_name);

    if (maybe_texture.is_none()) {
        LOG_WARN("Trying to delete texture that does not exist: {}", texture_name);
        return;
    }

    TextureRef* texture_ref{ maybe_texture.unwrap_copy() };
    Texture& texture{ state_ptr->textures[texture_ref->handle] };
    texture_ref->ref_count--;

    if (texture_ref->ref_count == 0 && texture_ref->auto_release) {
        texture_ref->handle = INVALID_ID;
        texture.destroy(device); 
        state_ptr->texture_lookup_table.remove(texture_name);
    }
}

} // sf
