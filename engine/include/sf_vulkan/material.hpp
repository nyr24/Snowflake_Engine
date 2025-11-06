#pragma once

#include "glm/ext/vector_float4.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/image.hpp"
#include "sf_vulkan/texture.hpp"
#include <string_view>

namespace sf {

inline constexpr u32 MATERIAL_NAME_MAX_LEN{ 64 };

struct MaterialConfig {
    static constexpr u32 PROP_COUNT{4};
    static constexpr u32 MAX_STR_LEN{ 128 };
    
    FixedArray<char, MATERIAL_NAME_MAX_LEN> name;
    FixedArray<char, TEXTURE_NAME_MAX_LEN>  texture_map_name;
    glm::vec4        diffuse_color;
    bool             auto_release;
};

// TODO: not sure why need internal_id, generation for
struct Material {
    TextureMap                              diffuse_map;
    u32                                     id{INVALID_ID};
    u32                                     generation{INVALID_ID};
    MaterialConfig                          config;
public:
    void destroy();
};

struct MaterialRef {
    u32  handle{INVALID_ID};
    u32  ref_count{0};
    bool auto_release{false};  
};

struct MaterialSystemState {
    static constexpr std::string_view DEFAULT_NAME{ "default_mat.sfmt" };
    static constexpr u32 MAX_MATERIAL_AMOUNT{ 4096 };
    using MaterialHashMap = HashMap<std::string_view, MaterialRef, LinearAllocator>;

    DynamicArray<Material, LinearAllocator>    materials;
    MaterialHashMap                            material_lookup_table;
    u32                                        id_counter;

    static consteval u32 get_memory_requirement() { return MAX_MATERIAL_AMOUNT * sizeof(Material) + MAX_MATERIAL_AMOUNT * sizeof(MaterialHashMap::Bucket); }
    static void create(LinearAllocator& system_allocator, MaterialSystemState& out_system);
    ~MaterialSystemState();
};

void material_system_init_internal_state(MaterialSystemState* state);
void material_system_load_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<std::string_view> file_names, std::span<Material*> out_materials);
void material_system_load_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<MaterialConfig> configs, std::span<Material*> out_materials);
Material* material_system_get_or_load_from_config(MaterialConfig& config, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer);
Material* material_system_get_or_load_from_file(std::string_view file_name, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer);
void material_system_free_material(std::string_view name);

} // sf
