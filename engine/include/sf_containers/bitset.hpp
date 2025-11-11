#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include <cmath>

namespace sf {

inline constexpr u16 bitset_get_bit_size(u16 max_bit) { return static_cast<u16>(std::ceil(max_bit / 64.0f)); }

template<u16 BIT_SIZE>
struct BitSet {
    static_assert(BIT_SIZE % 64 == 0, "Should be divisible by 64");
    static constexpr u16 BIT_BUCKETS{ static_cast<u16>(BIT_SIZE / 64) };

    BitSet()
    {
        reset();
    }

    u64 data[BIT_BUCKETS];

    void set_bit(u16 bit) {
        data[bit >> 6] |= (1ULL << (bit & 63));
    }

    void unset_bit(u16 bit) {
        data[bit >> 6] &= (0ULL << (bit & 63));
    }

    void toggle_bit(u16 bit) {
        data[bit >> 6] ^= (1ULL << (bit & 63));
    }
    
    bool is_bit(u16 bit) {
        return data[bit >> 6] & (1ULL << (bit & 63));
    }

    void reset() {
        sf_mem_zero(data, BIT_BUCKETS * sizeof(u64));
    }
};

} // sf


