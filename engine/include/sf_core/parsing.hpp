#pragma once

#include "glm/ext/vector_float4.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/result.hpp"
#include "sf_containers/traits.hpp"
#include <cctype>
#include <string_view>

namespace sf {

template<AllocatorTrait Allocator>
struct Parser {
public:
    using CheckCharFn = i32(*)(i32) noexcept;
    const StringBacked<Allocator>& str;
    u32 offset;
public:
    Parser(const StringBacked<Allocator>& str_in)
        : str{ str_in }
        , offset{0}
    {}

    template<u32 MAX_LEN>
    bool parse_until(char terminator, FixedArray<char, MAX_LEN>& result) {
        u32 i{offset};
        while (i < str.count()) {
            if (str[i] == terminator) {
                u32 len{ i - offset };
                result = str.to_span(offset, len);
                offset += len;
                return true;
            }
            i++;
        }
        return false;
    }

    bool parse_until_no_store(char terminator) {
        u32 i{offset};
        while (i < str.count()) {
            if (str[i] == terminator) {
                offset += (i - offset);
                return true;
            }
            i++;
        }
        return false;
    }

    void skip_ws() {
        while (offset < str.count() && std::isspace(str[offset])) {
            ++offset;
        }
    }

    void skip_until_callback(CheckCharFn cb) {
        while (offset < str.count() && !cb(str[offset])) {
            ++offset;
        }
    }

    bool vec4_from_str(std::string_view str, glm::vec4& out_vec) {
        i32 res = std::sscanf(str.data(), "%f %f %f %f", &out_vec.r, &out_vec.g, &out_vec.b, &out_vec.a);
        return (res == 0) ? false : true;
    }

    Result<bool> bool_from_str(std::string_view str) {
        static constexpr std::string_view TRUE_S = "true";
        static constexpr std::string_view FALSE_S = "false";

        if (str == TRUE_S) {
            return true;
        } else if (str == FALSE_S) {
            return false;
        } else {
            return {ResultError::VALUE};
        }
    }
};

} // sf
