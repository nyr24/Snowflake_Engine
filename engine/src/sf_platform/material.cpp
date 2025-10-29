#include "sf_vulkan/material.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/io.hpp"
#include "sf_core/application.hpp"

namespace sf {

FixedArray<std::string_view, MaterialConfig::PROP_COUNT> config_property_names{
    "name", "texture_map_name", "diffuse_color", "auto_release"
};

static MaterialSystemState* state_ptr{nullptr};

void MaterialSystemState::create(LinearAllocator& system_allocator, MaterialSystemState& out_system) {
    out_system.materials.set_allocator(&system_allocator);
    out_system.material_lookup_table.set_allocator(&system_allocator);
    out_system.materials.resize(MAX_MATERIAL_AMOUNT);
    out_system.material_lookup_table.reserve(MAX_MATERIAL_AMOUNT);
}

void material_system_init_internal_state(MaterialSystemState* state) {
    state_ptr = state;
}

MaterialSystemState::~MaterialSystemState()
{
    for (auto& m : materials) {
        // TODO: destroy materials
        // m.destroy();
    }
}

static void material_parse_config(std::string_view file_name, MaterialConfig& out_config);

void material_system_acquire_from_file();

void material_system_acquire_from_config();

static void material_parse_config(std::string_view file_name, MaterialConfig& out_config) {
// TODO: add build task to copy materials from "/assets/materials" to curr build directory
#ifdef SF_DEBUG
    fs::path file_path = fs::current_path() / "build" / "debug/engine/materials/" / file_name;
#else
    fs::path file_path = fs::current_path() / "build" / "release/engine/materials/" / file_name;
#endif

    auto read_res = read_file(file_path, application_get_temp_allocator());
    if (read_res.is_err()) {
        LOG_ERROR("Material System: can't read file from path: {}", file_path.c_str());
        return;
    }
}

} // sf
