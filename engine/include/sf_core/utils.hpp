#pragma once
#include <format>

namespace sf {
template<typename T>
constexpr bool is_power_of_two(T x) {
    return (x & (x-1)) == 0;
}

// TODO: make it work
template<typename T, typename CharT>
concept HasFormatter = requires(std::format_parse_context ctx) {
    std::formatter<T, CharT>::format(std::declval<T>(), ctx);
};
} // sf
