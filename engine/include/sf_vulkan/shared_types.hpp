#pragma once

#include <string_view>

namespace sf {

struct Mesh;
struct VulkanContext;
struct VulkanDevice;
struct VulkanCommandBuffer;
struct VulkanShaderPipeline;
struct Texture;
struct Material;
struct Geometry;

struct TextureInputConfig {
    std::string_view file_name;
    bool             auto_release;

    TextureInputConfig() = default;
    TextureInputConfig(std::string_view file_name): file_name{ file_name }, auto_release{false} {};
};

} // sf

