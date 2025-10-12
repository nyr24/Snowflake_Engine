#pragma once

#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/image.hpp"
#include <climits>
#include <vulkan/vulkan_core.h>

namespace sf {

// TODO: move to shared constants
constexpr u32 INVALID_ID{ UINT_MAX };

struct VulkanDevice;
struct VulkanCommandBuffer;

enum struct ImageFormat : u8 {
    JPG,
    PNG,
    BITMAP,
    COUNT
};

const FixedArray<std::string_view, static_cast<u32>(ImageFormat::COUNT)> image_extensions{ "jpg", "png", "bmp" };

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
    bool              has_transparency;
    u8                channel_count; 
    ImageFormat       format;

public:
    static bool load(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer,
        const char* file_name,
        Texture& out_texture
    );
    static ImageFormat map_extension_to_format(std::string_view extension);
    void destroy(const VulkanDevice& device);
    bool upload_to_gpu(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer
    );
private:
    static bool load_from_disk(
        const char* file_name,
        Texture& out_texture
    );
};

} // sf
