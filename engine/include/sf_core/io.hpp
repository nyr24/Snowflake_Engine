#pragma once

#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
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

template<u32 ARR_LEN>
FixedArray<char, ARR_LEN> strip_extension_from_file_name(FixedArray<char, ARR_LEN>& file_name) {
    u32 last_dot_ind{0};
    u32 curr{0};

    while (curr < file_name.count()) {
        if (file_name[curr] == '.') {
            last_dot_ind = curr;
        }
        ++curr;
    }

    if (last_dot_ind == 0) {
        LOG_WARN("File name {} don't have extension, nothing to strip", file_name.to_string_view());
        return file_name;
    } else {
        file_name.pop_range(file_name.count() - last_dot_ind);
    }

    return file_name;
}


std::string_view extract_extension_from_file_name(std::string_view file_name);
std::string_view strip_extension_from_file_name(std::string_view file_name);

} // sf
