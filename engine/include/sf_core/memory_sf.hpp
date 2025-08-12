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

u32 sf_calc_padding(void* address, u16 alignment);
bool is_address_in_range(void* start, u32 total_size, void* addr);
u32 ptr_diff(void* ptr1, void* ptr2);
u32 turn_ptr_into_handle(void* ptr, void* start);
u32 calc_padding_with_header(void* ptr, u16 alignment, u16 header_size);

template<typename ReturnPtr = void*>
ReturnPtr turn_handle_into_ptr(u32 handle, void* start) {
    return reinterpret_cast<ReturnPtr>(reinterpret_cast<u8*>(start) + handle);
}

template<typename ReturnPtr = void*>
ReturnPtr rebase_ptr(void* old_ptr, void* old_base, void* new_base) {
    return reinterpret_cast<ReturnPtr>(turn_handle_into_ptr(turn_ptr_into_handle(old_ptr, old_base), new_base));
}

template<typename T>
T* ptr_step_bytes_forward(T* ptr, u32 byte_count) {
    return reinterpret_cast<T*>(reinterpret_cast<u8*>(ptr) + byte_count);
}

template<typename T>
T* ptr_step_bytes_backward(T* ptr, u32 byte_count) {
    return reinterpret_cast<T*>(reinterpret_cast<u8*>(ptr) - byte_count);
}

template<typename T>
T* sf_align_forward(T* address, u16 alignment) {
    usize addr = reinterpret_cast<usize>(address);
    return reinterpret_cast<T*>((addr + (alignment - 1)) & ~(alignment - 1));
}

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
