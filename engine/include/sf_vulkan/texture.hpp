#pragma once

#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/hashmap.hpp"
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

inline constexpr u32 TEXTURE_NAME_MAX_LEN{ 64 };

enum struct TextureUse : u8 {
    UNKNOWN,
    MAP_DIFFUSE 
};

struct Texture {
public:
    VulkanImage       image;
    VulkanBuffer      staging_buffer;
    VkSampler         sampler;
    u8*               pixels;
    u32               width;
    u32               height;
    u32               size;
    u32               id{INVALID_ID};
    u32               generation{INVALID_ID};
    bool              has_transparency;
    u8                channel_count; 
    ImageFormat       format;
    TextureState      state{TextureState::NOT_LOADED};

public:
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

struct TextureRef {
    u32               handle;
    u32               ref_count;
    bool              auto_release;
};

struct TextureSystemState {
public:
    static constexpr u32 MAX_TEXTURE_AMOUNT{ 65536 };
    using TextureHashMap = HashMap<std::string_view, TextureRef, LinearAllocator, MAX_TEXTURE_AMOUNT>;

    DynamicArray<Texture, LinearAllocator, MAX_TEXTURE_AMOUNT>                 textures;
    TextureHashMap                                                             texture_lookup_table;
    const VulkanDevice*    device;
    u32                    id_counter;
public:
    static consteval u32 get_memory_requirement() { return MAX_TEXTURE_AMOUNT * sizeof(Texture) + MAX_TEXTURE_AMOUNT * sizeof(TextureHashMap::Bucket); }
    static void create(LinearAllocator& allocator, const VulkanDevice& device, TextureSystemState& out_system);
    ~TextureSystemState();
};

struct TextureMap {
    Texture*   texture;
    TextureUse use; 
};

void texture_system_init_internal_state(TextureSystemState* state);
void texture_system_load_textures(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<const TextureInputConfig> configs, std::span<Texture*> out_textures);
Texture* texture_system_get_or_load_texture(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, const TextureInputConfig config);
Texture* texture_system_get_texture(std::string_view name);
void texture_system_free_texture(const VulkanDevice& device, std::string_view name);

} // sf
