#pragma once
#include "sf_core/defines.hpp"
#include "sf_platform/platform_mem_alloc_templated.hpp"
#include <new>
#include <utility>

namespace sf {

// non-templated versions of memory functions (needed for void*)
SF_EXPORT void* sf_mem_alloc(usize byte_size, u16 alignment = 0);
SF_EXPORT void* sf_mem_realloc(void* ptr, usize byte_size);
SF_EXPORT void  sf_mem_free(void* block, u16 alignment = 0);
SF_EXPORT void  sf_mem_set(void* block, usize byte_size, i32 value);
SF_EXPORT void  sf_mem_zero(void* block, usize byte_size);
SF_EXPORT void  sf_mem_copy(void* dest, void* src, usize byte_size);
SF_EXPORT void  sf_mem_move(void* dest, void* src, usize byte_size);
SF_EXPORT bool  sf_mem_cmp(void* first, void* second, usize byte_size);
SF_EXPORT bool  sf_str_cmp(const char* first, const char* second);

// templated versions of memory functions
template<typename T, bool should_align>
SF_EXPORT T* sf_mem_alloc_typed(usize count) {
    return platform_mem_alloc_typed<T, should_align>(count);
}

template<typename T>
SF_EXPORT T* sf_mem_realloc_typed(T* ptr, usize count) {
    return static_cast<T*>(sf_mem_realloc(ptr, sizeof(T) * count));
}

template<typename T, bool should_align>
void sf_mem_free_typed(T* block) {
    if constexpr (should_align) {
        ::operator delete(block, static_cast<std::align_val_t>(alignof(T)), std::nothrow);
    } else {
        ::operator delete(block, std::nothrow);
    }
}

template<typename T, typename... Args>
T* sf_mem_construct(Args&&... args) {
    return new (std::nothrow) T(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T* sf_mem_place(T* ptr, Args&&... args) {
    return new (ptr) T(std::forward<Args>(args)...);
}

} // sf
