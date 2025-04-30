#pragma once
#include <cstdlib>
#include "sf_types.hpp"
#include "logger.hpp"

template<typename T>
T* sf_alloc(usize size) {
    T* res = static_cast<T*>(malloc(size));
    if (!res) {
        LOG_FATAL("Allocation failed, bytes needed: {}", size);
        std::exit(1);
    }

    return res;
}
