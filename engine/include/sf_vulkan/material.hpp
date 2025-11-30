#pragma once

#include "glm/ext/vector_float4.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/shared_types.hpp"
#include "sf_vulkan/texture.hpp"
#include "sf_vulkan/pipeline.hpp"
#include <span>
#include <string_view>

namespace sf {

inline constexpr u32 MATERIAL_NAME_MAX_LEN{ 64 };

struct MaterialConfig {
    static constexpr u32 PROP_COUNT{4};
    static constexpr u32 MAX_STR_LEN{ 128 };
    
    FixedString<MATERIAL_NAME_MAX_LEN> name;
    FixedString<TEXTURE_NAME_MAX_LEN>  diffuse_texture_name;
    glm::vec4        diffuse_color;
    bool             auto_release;
};

struct TextureMap {
    Texture*      texture;
    TextureType   type;  
};

template<u32 CAPACITY>
struct TextureMaps {
    FixedArray<Texture*, CAPACITY>      textures;
    FixedArray<TextureType, CAPACITY>   types;

    TextureMaps()
    {
        textures.resize_to_capacity();
        types.resize_to_capacity();
    }

    void append(TextureMap map) {
        textures.append(map.texture);
        types.append(map.type);
    }

    void resize_to_capacity() {
        textures.resize_to_capacity();
        types.resize_to_capacity();
    }

    void resize(u32 val) {
        textures.resize(val);
        types.resize(val);
    }

    u32 count() {
        SF_ASSERT(textures.count() == types.count());
        return textures.count();
    }

    void clear() {
        textures.clear();
        types.clear();
    }

    TextureMap operator[](u32 i) {
        SF_ASSERT_MSG(i >= 0 && i < this->count(), "Out of bounds");
        return TextureMap{ textures[i], types[i] };
    }
};

struct Material {
    static constexpr glm::vec4 DEFAULT_DIFFUSE_COLOR{ 1.0f, 1.0f, 1.0f, 0.0f }; 
    
    // SOA
    TextureMaps<VulkanShaderPipeline::TEXTURE_COUNT> texture_maps;
    glm::vec4                                        diffuse_color{ DEFAULT_DIFFUSE_COLOR };
    Option<MaterialConfig>                           config{None::VALUE};
    u32                                              id{INVALID_ID};
    u32                                              generation{INVALID_ID};
public:
    static void create_empty(u32 id, Material& out_material);
    void destroy();
};

struct MaterialRef {
    u32  handle{INVALID_ID};
    u32  ref_count{0};
    bool auto_release{false};  
};

struct MaterialSystem {
public:
    static constexpr std::string_view DEFAULT_FILE_NAME{ "default_mat.sfmt" };
    static constexpr u32 INIT_MATERIAL_AMOUNT{ 4096 };

    using MaterialHashMap = HashMap<std::string_view, MaterialRef, ArenaAllocator, false>;
    DynamicArray<Material, ArenaAllocator, false>       materials;
    MaterialHashMap                                     material_lookup_table;
public:
    static void create(ArenaAllocator& system_allocator, MaterialSystem& out_system);
    static consteval u32 get_memory_requirement() { return INIT_MATERIAL_AMOUNT * sizeof(Material) + INIT_MATERIAL_AMOUNT * sizeof(MaterialHashMap::Bucket); }
    ~MaterialSystem();
    static void preload_material_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<std::string_view> file_names);
    static void preload_material_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<MaterialConfig> configs);
    static void load_and_get_material_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<std::string_view> file_names, std::span<Material*> out_materials);
    static void load_and_get_material_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<MaterialConfig> configs, std::span<Material*> out_materials);
    static Material* load_and_get_material_from_config(MaterialConfig&& config, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
    static Material* load_and_get_material_from_file(std::string_view file_name, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
    static Material& create_material_from_textures(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer,
        StackAllocator& alloc,
        std::span<TextureInputConfig> tex_configs
    );
    static void free_material(std::string_view name);
    static Material& get_empty_slot();
    static Material& get_default_material();
    static void create_default_material(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
private:
    void resize();
};

} // sf
