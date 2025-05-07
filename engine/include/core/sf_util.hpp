#pragma once
#include <cstdlib>
#include "core/sf_logger.hpp"

template<typename T>
T* sf_alloc(usize count, bool aligned) {
    if (aligned) {
        alignas(T) T* res = static_cast<T*>(malloc(sizeof(T) * count));
        if (!res) {
            LOG_FATAL("Allocation failed");
            std::exit(1);
        }

        return res;
    } else {
        T* res = static_cast<T*>(malloc(sizeof(T) * count));
        if (!res) {
            LOG_FATAL("Allocation failed");
            std::exit(1);
        }

        return res;
    }
}
