#pragma once

#include "sf_core/defines.hpp"

namespace sf {
void* platform_mem_alloc(u64 byte_size, u16 alignment);

template<typename T, bool should_align>
T* platform_mem_alloc_typed(u64 count) {
    if constexpr (should_align) {
        return static_cast<T*>(platform_mem_alloc(sizeof(T) * count, alignof(T)));
    } else {
        return static_cast<T*>(platform_mem_alloc(sizeof(T) * count, 0));
    }
}
}
