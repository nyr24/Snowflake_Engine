#pragma once

#include "glm/ext/vector_float4.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
// #include "sf_containers/optional.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/shared_types.hpp"
#include "sf_vulkan/texture.hpp"
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

struct Material {
    static constexpr u32 MAX_DIFFUSE_COUNT{ 8 };
    static constexpr u32 MAX_SPECULAR_COUNT{ 4 };
    static constexpr u32 MAX_AMBIENT_COUNT{ 4 };
    static constexpr glm::vec4 DEFAULT_DIFFUSE_COLOR{ 1.0f, 1.0f, 1.0f, 1.0f }; 
    
    FixedArray<Texture*, MAX_DIFFUSE_COUNT>     diffuse_textures;
    FixedArray<Texture*, MAX_SPECULAR_COUNT>    specular_textures;
    FixedArray<Texture*, MAX_AMBIENT_COUNT>     ambient_textures;
    // THINK: should ve have optional?
    // Option<glm::vec4>                           diffuse_color{None::VALUE};
    glm::vec4                                   diffuse_color{ DEFAULT_DIFFUSE_COLOR };
    Option<MaterialConfig>                      config{None::VALUE};
    u32                                         id{INVALID_ID};
    u32                                         generation{INVALID_ID};
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
    static constexpr std::string_view DEFAULT_NAME{ "default_mat.sfmt" };
    static constexpr u32 INIT_MATERIAL_AMOUNT{ 4096 };

    using MaterialHashMap = HashMapBacked<std::string_view, MaterialRef, ArenaAllocator, false>;
    DynamicArrayBacked<Material, ArenaAllocator, false> materials;
    Material                                            default_material;
    MaterialHashMap                                     material_lookup_table;
public:
    static void create(ArenaAllocator& system_allocator, MaterialSystem& out_system);
    static consteval u32 get_memory_requirement() { return INIT_MATERIAL_AMOUNT * sizeof(Material) + INIT_MATERIAL_AMOUNT * sizeof(MaterialHashMap::Bucket); }
    ~MaterialSystem();
    static void load_material_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<std::string_view> file_names, std::span<Material*> out_materials);
    static void load_material_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<MaterialConfig> configs, std::span<Material*> out_materials);
    static Material* get_or_load_material_from_config(MaterialConfig&& config, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
    static Material* get_or_load_material_from_file(std::string_view file_name, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
    static Material& create_material_from_textures(
        const VulkanDevice& device,
        VulkanCommandBuffer& cmd_buffer,
        StackAllocator& alloc,
        std::span<TextureInputConfig> diffuse_tex_configs,
        std::span<TextureInputConfig> specular_tex_configs,
        std::span<TextureInputConfig> ambient_tex_configs
    );
    static void free_material(std::string_view name);
    static Material& get_empty_slot();
    static Material& get_default_material();
    static void create_default_material(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc);
private:
    void resize();
};

} // sf
