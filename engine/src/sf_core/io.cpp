#include "sf_core/io.hpp"
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
        return file_name;
    }

    return file_name.substr(0, last_dot_ind);
}

std::string_view strip_file_name_from_path(std::string_view file_path) {
    u32 curr{0};    
    u32 last_path_sep{0};
    
    while (curr < file_path.size()) {
        if (file_path[curr] == '/') {
            last_path_sep = curr;
        }
        ++curr;    
    }

    return file_path.substr(0, last_path_sep + 1);
}

std::string_view trim_dir_and_extension_from_path(std::string_view file_path) {
    u32 curr{0};
    u32 from{0};
    u32 to{0};
    
    while (curr < file_path.size()) {
        if (file_path[curr] == '/') {
            from = curr;
        }
        ++curr;    
    }

    from = (from == 0) ? from : from + 1;
    curr = from;

    while (curr < file_path.size()) {
        if (file_path[curr] == '.') {
            to = curr;
        }
        ++curr;
    }

    if (to == 0) {
        to = curr;
    }

    return file_path.substr(from, to - from);
}

std::string_view strip_part_from_start_and_extension(std::string_view file_path, std::string_view part) {
    u32 part_sz{static_cast<u32>(part.size())};
    u32 file_path_sz{static_cast<u32>(file_path.size())};

    if (part_sz > file_path_sz) {
        return file_path;
    }

    u32 from{0};

    while (from < part_sz && file_path[from] == part[from]) {
        ++from;
    }

    u32 curr{from};
    u32 to{from};

    while (curr < file_path_sz) {
        if (file_path[curr] == '.') {
            to = curr;   
        }
        ++curr;
    }

    if (to == from) {
        to = file_path_sz;
    }

    return file_path.substr(from, to - from);
}

} // sf
