#pragma once

namespace sf {
template<typename T>
constexpr bool is_power_of_two(T x) {
    return (x & (x-1)) == 0;
}
} // sf
