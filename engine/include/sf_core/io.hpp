#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_containers/traits.hpp"
#include <fstream>
#include <filesystem>


namespace sf {

namespace fs = std::filesystem;

template<AllocatorTrait Allocator>
Result<DynamicArray<char, Allocator>> read_file(const fs::path& file_path, Allocator& allocator) noexcept {
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return {ResultError::VALUE};
    }

    DynamicArray<char, Allocator> file_contents(file.tellg(), file.tellg(), &allocator);
    file.seekg(0, std::ios::beg);
    file.read(file_contents.data(), static_cast<std::streamsize>(file_contents.count()));

    return {std::move(file_contents)};
}

std::string_view extract_extension_from_file_path(std::string_view file_name);
std::string_view strip_extension_from_file_path(std::string_view file_name);

} // sf
