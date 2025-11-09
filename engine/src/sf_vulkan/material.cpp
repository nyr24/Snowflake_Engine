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
#include "sf_core/parsing.hpp"
#include "sf_vulkan/texture.hpp"
#include <span>
#include <string_view>

namespace sf {

void Material::destroy() {
    id = INVALID_ID;
    generation = INVALID_ID;
}

// NOTE: must be ordered as in MaterialConfig struct
FixedArray<std::string_view, MaterialConfig::PROP_COUNT> config_property_names{
    "name", "texture_map_name", "diffuse_color", "auto_release"
};

static MaterialSystemState* state_ptr{nullptr};

void MaterialSystemState::create(ArenaAllocator& system_allocator, MaterialSystemState& out_system) {
    out_system.materials.set_allocator(&system_allocator);
    out_system.material_lookup_table.set_allocator(&system_allocator);
    out_system.materials.resize(MAX_MATERIAL_AMOUNT);
    out_system.material_lookup_table.reserve(MAX_MATERIAL_AMOUNT);

    for (auto& m : out_system.materials) {
        m.id = INVALID_ID;
        m.generation = INVALID_ID;
    }
    for (auto& m : out_system.material_lookup_table) {
        m.value.handle = INVALID_ID;
    }
}

void material_system_init_internal_state(MaterialSystemState* state) {
    state_ptr = state;
}

MaterialSystemState::~MaterialSystemState()
{
    for (auto& m : materials) {
        m.destroy();
    }
}

static bool material_parse_config(std::string_view file_name, MaterialConfig& out_config);
static bool is_all_config_parsed(u8 parsed_state);
static u32 get_new_material_id();

void material_system_load_from_file_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<std::string_view> file_names, std::span<Material*> out_materials) {
    SF_ASSERT(file_names.size() == out_materials.size());
    for (u32 i{0}; i < file_names.size(); ++i) {
       out_materials[i] = material_system_get_or_load_from_file(file_names[i], device, cmd_buffer);
    }
}

void material_system_load_from_config_many(const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer, std::span<MaterialConfig> configs, std::span<Material*> out_materials) {
    SF_ASSERT(configs.size() == out_materials.size());
    for (u32 i{0}; i < configs.size(); ++i) {
       out_materials[i] = material_system_get_or_load_from_config(configs[i], device, cmd_buffer);
    }
}

Material* material_system_get_or_load_from_file(std::string_view file_name, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer) {
    MaterialConfig config;
    bool parse_res = material_parse_config(file_name, config);
    if (!parse_res) {
        return nullptr;
    }
    return material_system_get_or_load_from_config(config, device, cmd_buffer);
}

Material* material_system_get_or_load_from_config(MaterialConfig& config, const VulkanDevice& device, VulkanCommandBuffer& cmd_buffer) {
    Option<MaterialRef*> maybe_existing_material = state_ptr->material_lookup_table.get(config.name.to_string_view());
    if (maybe_existing_material.is_some()) {
        MaterialRef* existing_material = maybe_existing_material.unwrap_copy();
        existing_material->ref_count++;
        return &state_ptr->materials[existing_material->handle];
    }

    Material new_material{
        .id = get_new_material_id(),
        .generation = 0,
        .config = config,
    };

    TextureInputConfig texture_conf{ config.texture_map_name.to_string_view() };
    new_material.diffuse_map = {
        .texture = texture_system_get_or_load_texture(device, cmd_buffer, texture_conf),
        .use = TextureUse::MAP_DIFFUSE
    };

#ifdef SF_DEBUG
    if (!new_material.diffuse_map.texture) {
        LOG_ERROR("Material {} can not acquire texture with name: {}, file is missing", config.name.to_string_view(), config.texture_map_name.to_string_view());
    }
#endif

    u32 handle{0};
    for (u32 i{0}; i < state_ptr->materials.capacity(); ++i) {
        if (state_ptr->materials[i].id == INVALID_ID) {
            state_ptr->materials[i] = new_material;
            handle = i;
        }
    }

    if (handle >= state_ptr->materials.capacity()) {
        LOG_ERROR("Not enough space to load texture: {}", config.name.to_string_view());
        return nullptr;
    }

    MaterialRef ref{
        .handle = handle,
        .ref_count = 0,
        .auto_release = config.auto_release,    
    };

    state_ptr->material_lookup_table.put(new_material.config.name.to_string_view(), ref);
    return &state_ptr->materials[ref.handle];
}

void material_system_free_material(std::string_view name_input) {
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

static bool material_parse_config(std::string_view file_name, MaterialConfig& out_config) {
#ifdef SF_DEBUG
    fs::path file_path = fs::current_path() / "build" / "debug/engine/assets/materials/" / file_name;
#else
    fs::path file_path = fs::current_path() / "build" / "release/engine/assets/materials/" / file_name;
#endif
    auto maybe_file_contents = read_file(file_path, application_get_temp_allocator());
    if (maybe_file_contents.is_err()) {
        LOG_ERROR("Material System: can't read file from path: {}", file_path.c_str());
        return false;
    }

    StringBacked<StackAllocator> file_contents{ maybe_file_contents.unwrap_move() };    

    FixedArray<char, MaterialConfig::MAX_STR_LEN> line_buff;
    Parser<StackAllocator> parser{ file_contents };
    // bitwise
    u8 parsed_state{0};

    while (parser.offset < file_contents.count() && !is_all_config_parsed(parsed_state)) {
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
                out_config.texture_map_name = line_buff.to_span();
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

static u32 get_new_material_id() {
    return state_ptr->id_counter++;
}

} // sf
