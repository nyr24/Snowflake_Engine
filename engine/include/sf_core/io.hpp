#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include <filesystem>

namespace sf {

Result<DynamicArray<char>> read_file(std::filesystem::path file_name) noexcept;

} // sf
