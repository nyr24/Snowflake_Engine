#pragma once

#include "sf_core/defines.hpp"
#include "sf_vulkan/image.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanContext;
struct VulkanFence;

struct Texture {
public:
    VulkanImage       image;
    VkSampler         sampler;
    u8*               pixels;
    u32               id;
    u32               width;
    u32               height;
    u32               generation;
    bool              has_transparency;
    // THINK: maybe get this info from channel_count?
    u8                channel_count; 

public:
    static bool create(
        const VulkanContext& context,
        const char* file_name,
        bool has_transparency,
        Texture& out_texture
    );

    void destroy(const VulkanDevice& device);
};

} // sf
