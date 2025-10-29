#pragma once

#include "glm/ext/vector_float4.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_vulkan/texture.hpp"
#include <string_view>

namespace sf {

inline constexpr u32 MATERIAL_NAME_MAX_LEN{ 64 };

struct MaterialConfig {
    static constexpr u32 PROP_COUNT{4};
    
    FixedArray<char, MATERIAL_NAME_MAX_LEN> name;
    FixedArray<char, TEXTURE_NAME_MAX_LEN>  texture_map_name;
    glm::vec4        diffuse_color;
    bool             auto_release;
};

struct Material {
    u32 id{INVALID_ID};
    u32 internal_id{INVALID_ID};
    u32 generation{INVALID_ID};
    std::string_view name;
    glm::vec4        diffuse_color;
    TextureMap       diffuse_map;
};

struct MaterialRef {
    u32  ref_count;
    u32  handle;
    bool auto_release;  
};

struct MaterialSystemState {
    static constexpr u32 MAX_MATERIAL_AMOUNT{ 4096 };
    using MaterialHashMap = HashMap<std::string_view, MaterialRef, LinearAllocator, MAX_MATERIAL_AMOUNT>;

    DynamicArray<Material, LinearAllocator, MAX_MATERIAL_AMOUNT>                 materials;
    MaterialHashMap                                                              material_lookup_table;
    // TODO:
    // Material                            default_material;
    // const VulkanDevice* device;

    static consteval u32 get_memory_requirement() { return MAX_MATERIAL_AMOUNT * sizeof(Material) + MAX_MATERIAL_AMOUNT * sizeof(MaterialHashMap::Bucket); }
    static void create(LinearAllocator& system_allocator, MaterialSystemState& out_system);
    ~MaterialSystemState();
};

void material_system_init_internal_state(MaterialSystemState* state);
void material_system_acquire_from_config();
void material_system_acquire_from_file();

} // sf
