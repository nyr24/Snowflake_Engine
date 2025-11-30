#pragma once

#include "sf_core/defines.hpp"
#include <climits>
#include <string_view>

namespace sf {

inline constexpr u32 INVALID_ID{ UINT_MAX };
inline constexpr u32 INVALID_ALLOC_HANDLE{ UINT_MAX };
inline constexpr u32 VK_MAX_EXTENSION_COUNT{ 10 };
inline constexpr u32 MAX_FILE_NAME_LEN{ 100 };
inline constexpr std::string_view TEXTURE_ASSETS_PATH{ "build/debug/engine/assets/textures/" };
inline constexpr std::string_view MODEL_ASSETS_PATH{ "build/debug/engine/assets/models/" };
inline constexpr std::string_view MATERIAL_ASSETS_PATH{ "build/debug/engine/assets/materials/" };

}
