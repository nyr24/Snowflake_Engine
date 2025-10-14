#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/image.hpp"
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanDevice;
struct VulkanCommandBuffer;

enum struct ImageFormat : u8 {
    JPG,
    PNG,
    BITMAP,
    COUNT
};

const FixedArray<std::string_view, static_cast<u32>(ImageFormat::COUNT)> image_extensions{ "jpg", "png", "bmp" };

enum struct TextureState : u8 {
    NOT_LOADED,
    LOADED_FROM_DISK,
    UPLOADED_TO_GPU
};

struct TextureInputConfig {
    std::string_view file_name;
    bool             auto_release;

    TextureInputConfig() = default;
    TextureInputConfig(std::string_view file_name): file_name{ file_name }, auto_release{false} {};
};

struct Texture {
public:
    VulkanImage       image;
    VulkanBuffer      staging_buffer;
    VkSampler         sampler;
    u8*               pixels;
    u32               id;
    u32               width;
    u32               height;
    u32               size;
    u32               generation{INVALID_ID};
    u32               ref_count;
    bool              has_transparency;
    bool              auto_release;
    u8                channel_count; 
    ImageFormat       format;
    TextureState      state;

public:
    Texture();
    static bool load(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer,
        Texture& out_texture,
        TextureInputConfig input_config
    );
    static Result<ImageFormat> map_extension_to_format(std::string_view extension);
    void destroy(const VulkanDevice& device);
private:
    bool upload_to_gpu(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer
    );
    bool load_from_disk(std::string_view file_name);
};

void texture_system_create_textures(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<const TextureInputConfig> texture_configs);
Texture* texture_system_get_texture(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, const TextureInputConfig config);
bool texture_system_free_texture(const VulkanDevice& device, std::string_view name);

} // sf
