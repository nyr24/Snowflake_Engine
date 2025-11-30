#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/hashmap.hpp"
#include "sf_containers/result.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/constants.hpp"
#include "sf_vulkan/buffer.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/shared_types.hpp"
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace sf {

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

inline constexpr u32 TEXTURE_NAME_MAX_LEN{ 64 };

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
    static void create_empty(u32 id, Texture& out_texture);
    static bool load(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer,
        TextureInputConfig&& input_config,
        StackAllocator& alloc,
        Texture& out_texture
    );
    static Result<ImageFormat> map_extension_to_format(std::string_view extension);
    void destroy(const VulkanDevice& device);
private:
    bool upload_to_gpu(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer
    );
    bool load_from_disk(String<StackAllocator>&& file_name, StackAllocator& alloc);
};

struct TextureRef {
    u32               handle;
    u32               ref_count;
    bool              auto_release;
};

struct TextureSystem {
public:
    static constexpr u32 MAX_TEXTURE_AMOUNT{ 65536 };
#ifdef SF_DEBUG
    static constexpr std::string_view TEXTURE_FILE_PATH_TRIM_PART{"build/engine/debug/assets/"};
#else
    static constexpr std::string_view TEXTURE_FILE_PATH_TRIM_PART{"build/engine/release/assets/"};
#endif
    
    // storing hash of the string as key
    using TextureHashMap = HashMap<u64, TextureRef, ArenaAllocator, false>;

    DynamicArray<Texture, ArenaAllocator, false> textures;
    TextureHashMap                               texture_lookup_table;
    const VulkanDevice*    device;
    u32                    id_counter;
public:
    static consteval u32 get_memory_requirement() { return MAX_TEXTURE_AMOUNT * sizeof(Texture) + MAX_TEXTURE_AMOUNT * sizeof(TextureHashMap::Bucket); }
    static String<StackAllocator> acquire_default_texture_path(std::string_view texture_file_name, StackAllocator& alloc);
    static void create(ArenaAllocator& allocator, const VulkanDevice& device, TextureSystem& out_system);
    ~TextureSystem();
    static void get_or_load_textures_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc,  std::span<TextureInputConfig> configs, std::span<Texture*> out_textures);
    static Texture* get_or_load_texture(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, TextureInputConfig&& config, StackAllocator& alloc);
    static Texture* get_texture(std::string_view name);
    static void free_texture(const VulkanDevice& device, std::string_view name);
    static Texture& get_empty_slot();
};

} // sf
