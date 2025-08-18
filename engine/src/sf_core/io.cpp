#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include <filesystem>
#include <fstream>
#include <ios>

namespace sf {

Result<DynamicArray<char>> read_file(std::filesystem::path file_name) noexcept {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return {ResultError::VALUE};
    }

    DynamicArray<char> file_contents(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(file_contents.data(), static_cast<std::streamsize>(file_contents.count()));

    return file_contents;
}

} // sf
