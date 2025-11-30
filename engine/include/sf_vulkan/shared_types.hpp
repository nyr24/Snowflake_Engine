#pragma once

#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"

namespace sf {

struct Mesh;
struct VulkanContext;
struct VulkanDevice;
struct VulkanCommandBuffer;
struct VulkanShaderPipeline;
struct Texture;
struct Material;
struct GeometryView;

// same as aiTextureType
enum struct TextureType {
    NONE = 0,
    DIFFUSE = 1,
    SPECULAR = 2,
    AMBIENT = 3,
    EMISSIVE = 4,
    HEIGHT = 5,
    NORMALS = 6,
    SHININESS = 7,
    OPACITY = 8,
    DISPLACEMENT = 9,
    LIGHTMAP = 10,
    REFLECTION = 11,
    BASE_COLOR = 12,
    NORMAL_CAMERA = 13,
    EMISSION_COLOR = 14,
    METALNESS = 15,
    DIFFUSE_ROUGHNESS = 16,
    AMBIENT_OCCLUSION = 17,
    UNKNOWN = 18,
    SHEEN = 19,
    CLEARCOAT = 20,
    TRANSMISSION = 21,
    MAYA_BASE = 22,
    MAYA_SPECULAR = 23,
    MAYA_SPECULAR_COLOR = 24,
    MAYA_SPECULAR_ROUGHNESS = 25,
    ANISOTROPY = 26,
    GLTF_METALLIC_ROUGHNESS = 27,
};

struct TextureInputConfig {
    String<StackAllocator>       texture_path;
    bool                         auto_release;
    TextureType                  type;

    TextureInputConfig() = default;
    TextureInputConfig(String<StackAllocator>&& texture_path, TextureType type = TextureType::DIFFUSE, bool auto_release = false): texture_path{ std::move(texture_path) }, auto_release{ auto_release } {};
};

} // sf

