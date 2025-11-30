#include "sf_vulkan/material.hpp"
#include "glm/ext/vector_float4.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/io.hpp"
#include "sf_core/application.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/parsing.hpp"
#include "sf_vulkan/command_buffer.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/pipeline.hpp"
#include "sf_vulkan/texture.hpp"
#include <span>
#include <string_view>

namespace sf {

static MaterialSystem* state_ptr{nullptr};

void Material::create_empty(u32 id, Material& mat) {
    sf_mem_zero(&mat, sizeof(Material));
    mat.id = id;
    mat.generation = INVALID_ID;
}

void Material::destroy() {
    id = INVALID_ID;
    generation = INVALID_ID;
}

// NOTE: must be ordered as in MaterialConfig struct
FixedArray<std::string_view, MaterialConfig::PROP_COUNT> config_property_names{
    "name", "diffuse_texture_name", "diffuse_color", "auto_release"
};

void MaterialSystem::create(ArenaAllocator& system_allocator, MaterialSystem& out_system) {
    state_ptr = &out_system;
    out_system.materials.set_allocator(&system_allocator);
    out_system.material_lookup_table.set_allocator(&system_allocator);
    out_system.materials.resize(INIT_MATERIAL_AMOUNT);
    out_system.material_lookup_table.reserve(INIT_MATERIAL_AMOUNT);

    u32 count = out_system.materials.count();
    for (u32 i{0}; i < count; ++i) {
        auto& mat = out_system.materials[i];
        Material::create_empty(i, mat);
    }
}

void MaterialSystem::create_default_material(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc) {
    MaterialSystem::load_and_get_material_from_file(MaterialSystem::DEFAULT_FILE_NAME, device, cmd_buffer, alloc);
}

Material& MaterialSystem::get_default_material() {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");
    return state_ptr->materials[0];
}

Material& MaterialSystem::get_empty_slot() {
    SF_ASSERT_MSG(state_ptr, "Should be valid ptr");

    auto& materials = state_ptr->materials;
    u32 count{ materials.count() };
    
    for (u32 i{0}; i < count; ++i) {
        if (materials[i].generation == INVALID_ID) {
            materials[i].generation = 0;
            return materials[i];
        }
    }
    // empty slot not found, do resizing
    u32 free_slot = materials.count();
    materials.resize_exponent(materials.count() + 1);

    Material::create_empty(free_slot, materials[free_slot]);
    count = materials.count();

    for (u32 i{free_slot + 1}; i < count; ++i) {
        Material::create_empty(i, materials[i]);
    }

    return materials[free_slot];
}

MaterialSystem::~MaterialSystem()
{
    for (auto& m : materials) {
        m.destroy();
    }
}

static bool material_parse_config(std::string_view file_name, MaterialConfig& out_config, StackAllocator& alloc);
static bool is_all_config_parsed(u8 parsed_state);

Material& MaterialSystem::create_material_from_textures(
    const VulkanDevice& device,
    VulkanCommandBuffer& cmd_buffer,
    StackAllocator& alloc,
    std::span<TextureInputConfig> tex_configs
) {
    Material& new_mat = MaterialSystem::get_empty_slot();
    new_mat.texture_maps.resize(tex_configs.size());
    TextureSystem::get_or_load_textures_many(device, cmd_buffer, alloc, tex_configs, new_mat.texture_maps.textures.to_span());
    u32 loaded_count = new_mat.texture_maps.count();
    
    if (loaded_count < VulkanShaderPipeline::TEXTURE_COUNT && loaded_count > 0) {
        for (u32 i{ loaded_count }; i < VulkanShaderPipeline::TEXTURE_COUNT; ++i) {
            new_mat.texture_maps.append(new_mat.texture_maps[0]);            
        }
    }

    return new_mat;
}

void MaterialSystem::preload_material_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<std::string_view> file_names) {
    for (u32 i{0}; i < file_names.size(); ++i) {
       MaterialSystem::load_and_get_material_from_file(file_names[i], device, cmd_buffer, alloc);
    }
}
void MaterialSystem::preload_material_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<MaterialConfig> configs) {
    for (u32 i{0}; i < configs.size(); ++i) {
       MaterialSystem::load_and_get_material_from_config(std::move(configs[i]), device, cmd_buffer, alloc);
    }
}

void MaterialSystem::load_and_get_material_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<std::string_view> file_names, std::span<Material*> out_materials) {
    SF_ASSERT(file_names.size() == out_materials.size());
    for (u32 i{0}; i < file_names.size(); ++i) {
       out_materials[i] = MaterialSystem::load_and_get_material_from_file(file_names[i], device, cmd_buffer, alloc);
    }
}

void MaterialSystem::load_and_get_material_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc, std::span<MaterialConfig> configs, std::span<Material*> out_materials) {
    SF_ASSERT(configs.size() == out_materials.size());
    for (u32 i{0}; i < configs.size(); ++i) {
       out_materials[i] = MaterialSystem::load_and_get_material_from_config(std::move(configs[i]), device, cmd_buffer, alloc);
    }
}

Material* MaterialSystem::load_and_get_material_from_file(std::string_view file_name, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc) {
    MaterialConfig config;
    bool parse_res = material_parse_config(file_name, config, alloc);
    if (!parse_res) {
        return nullptr;
    }
    return MaterialSystem::load_and_get_material_from_config(std::move(config), device, cmd_buffer, alloc);
}

Material* MaterialSystem::load_and_get_material_from_config(MaterialConfig&& config, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, StackAllocator& alloc) {
    Option<MaterialRef*> maybe_existing_material_ref = state_ptr->material_lookup_table.get(config.name.to_string_view());
    if (maybe_existing_material_ref.is_some()) {
        MaterialRef* existing_material = maybe_existing_material_ref.unwrap_copy();
        existing_material->ref_count++;
        return &state_ptr->materials[existing_material->handle];
    }

    Material& new_mat = MaterialSystem::get_empty_slot();
    new_mat.config.set_some(std::move(config));

    TextureInputConfig texture_conf{ TextureSystem::acquire_default_texture_path(config.diffuse_texture_name.to_string_view(), alloc) };
    new_mat.texture_maps.append(TextureMap{ TextureSystem::get_or_load_texture(device, cmd_buffer, std::move(texture_conf), alloc) });

    MaterialRef ref{
        .handle = new_mat.id,
        .ref_count = 0,
        .auto_release = config.auto_release,    
    };

    state_ptr->material_lookup_table.put(new_mat.config.unwrap_ref().name.to_string_view(), ref);
    return &new_mat;
}

void MaterialSystem::free_material(std::string_view name_input) {
    auto name = strip_extension_from_file_name(name_input);
    Option<MaterialRef*> maybe_material_ref = state_ptr->material_lookup_table.get(name);
    if (maybe_material_ref.is_none()) {
        LOG_WARN("Trying to delete non-existing material: {}", name);
        return;
    }

    MaterialRef* material_ref = maybe_material_ref.unwrap_copy();
    Material& material = state_ptr->materials[maybe_material_ref.unwrap_copy()->handle];
    material_ref->ref_count--;

    if (material_ref->ref_count == 0 && material_ref->auto_release) {
        material_ref->handle = INVALID_ID;
        material.destroy();
        state_ptr->material_lookup_table.remove(name);
    }
}

static bool material_parse_config(std::string_view file_name, MaterialConfig& out_config, StackAllocator& alloc) {
#ifdef SF_DEBUG
    std::string_view init_path = "build/debug/engine/assets/materials/";
#else
    std::string_view init_path = "build/release/engine/assets/materials/";
#endif
    String<StackAllocator> material_path(init_path.size() + file_name.size() + 1, &alloc);
    material_path.append_sv(init_path);
    material_path.append_sv(file_name);
    material_path.append('\0');

    auto maybe_file_contents = read_file(material_path.to_sv_not_null_terminated(), application_get_temp_allocator());
    if (maybe_file_contents.is_err()) {
        LOG_ERROR("Material System: can't read file from path: {}", material_path.to_sv_not_null_terminated());
        return false;
    }

    String<StackAllocator> file_contents{ maybe_file_contents.unwrap_move() };    

    FixedString<MaterialConfig::MAX_STR_LEN> line_buff;
    Parser<StackAllocator> parser{ file_contents };
    // bitwise
    u8 parsed_state{0};

    while (!parser.end_reached() && !is_all_config_parsed(parsed_state)) {
        parser.skip_ws();

        // parse key
        bool key_parse_res = parser.parse_until('=', line_buff);
        if (!key_parse_res) {
            LOG_ERROR("Material config parsing error");
            return false;
        }

        Option<u32> maybe_prop_index = config_property_names.index_of(line_buff.to_string_view());
        if (maybe_prop_index.is_none()) {
            LOG_WARN("Material config unknown property name: {}", line_buff.to_string_view());
            parser.parse_until_no_store('\n');
            continue;
        }

        u32 prop_index = maybe_prop_index.unwrap_copy();
        line_buff.clear();
        parser.skip_until_callback(isalnum);

        // parse value
        bool val_parse_res = parser.parse_until('\n', line_buff);
        if (!val_parse_res) {
            LOG_ERROR("Material config parsing error");
            return false;
        }

        switch (prop_index) {
            case 0: {
                out_config.name = line_buff.to_span();
            } break;
            case 1: {
                out_config.diffuse_texture_name = line_buff.to_string_view();
            } break;
            case 2: {
                glm::vec4 vec;
                bool res = parser.vec4_from_str(line_buff.to_string_view(), vec);
                if (!res) {
                    LOG_ERROR("Failed to parse vec4 'diffuse_color' from material config");
                    out_config.diffuse_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    out_config.diffuse_color = vec;
                }
            } break;
            case 3: {
                Result<bool> res = parser.bool_from_str(line_buff.to_string_view());
                if (res.is_err()) {
                    LOG_ERROR("Failed to parse boolean 'auto_release' from material config"); 
                    out_config.auto_release = false;
                } else {
                    out_config.auto_release = res.unwrap_copy();
                }
            } break;
        }
        line_buff.clear();
        parsed_state |= 1 << prop_index;
    }

    return is_all_config_parsed(parsed_state);
}

static bool is_all_config_parsed(u8 parsed_state) {
    return (parsed_state & 0b1111) == 0b1111;
}

} // sf
