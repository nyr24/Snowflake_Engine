#include "sf_core/io.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include <string_view>

namespace sf {

std::string_view extract_extension_from_file_name(std::string_view file_name) {
    u32 last_dot_ind{0};
    u32 curr{0};

    while (curr < file_name.length()) {
        if (file_name[curr] == '.') {
            last_dot_ind = curr;
        }
        ++curr;
    }

    if (last_dot_ind == 0) {
        LOG_WARN("File name {} don't have extension, nothing to strip", file_name);
        return file_name;
    }

    return file_name.substr(last_dot_ind + 1);
}

std::string_view strip_extension_from_file_name(std::string_view file_name) {
    u32 last_dot_ind{0};
    u32 curr{0};

    while (curr < file_name.length()) {
        if (file_name[curr] == '.') {
            last_dot_ind = curr;
        }
        ++curr;
    }

    if (last_dot_ind == 0) {
        LOG_WARN("File name {} don't have extension, nothing to strip", file_name);
        return file_name;
    }

    return file_name.substr(0, last_dot_ind);
}

} // sf
