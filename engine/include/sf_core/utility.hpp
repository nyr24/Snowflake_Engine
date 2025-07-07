#pragma once
#include "sf_core/types.hpp"
#include <format>
#include <type_traits>

namespace sf {

[[noreturn]] void panic(const char* message);

template<typename T>
constexpr bool is_power_of_two(T x) {
    return (x & (x-1)) == 0;
}

// TODO: make it work
template<typename T, typename CharT>
concept HasFormatter = requires(std::format_parse_context ctx) {
    std::formatter<T, CharT>::format(std::declval<T>(), ctx);
};

template<typename T>
consteval bool smaller_than_word() {
    return sizeof(T) <= sizeof(usize);
}

template<typename T>
struct RefOrVal {
    using Type = std::conditional_t<smaller_than_word<T>(), T, T&>;
};

template<typename T>
struct ConstRefOrVal {
    using Type = std::conditional_t<smaller_than_word<T>(), T, const T&>;
};

template<typename First, typename Second>
concept SameTypes = std::same_as<std::remove_cv_t<std::remove_reference_t<First>>, std::remove_cv_t<std::remove_reference_t<Second>>>;

} // sf
