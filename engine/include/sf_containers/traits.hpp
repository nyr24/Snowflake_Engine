#pragma once

#include "sf_core/defines.hpp"
#include <concepts>
#include <utility>

namespace sf {

struct ReallocReturn {
    void*   ptr;
    // should container do memcpy yourself or not
    bool    should_mem_copy;
};

struct ReallocReturnHandle {
    usize   handle;
    // should container do memcpy yourself or not
    bool    should_mem_copy;
};

template<typename A, typename T = void, typename ...Args>
concept AllocatorTrait = requires(A a, Args... args) {
    { a.allocate(std::declval<usize>(), std::declval<u16>()) } -> std::same_as<T*>;
    { a.allocate_handle(std::declval<usize>(), std::declval<u16>()) } -> std::same_as<usize>;
    { a.handle_to_ptr(std::declval<usize>()) } -> std::same_as<T*>;
    { a.ptr_to_handle(std::declval<T*>()) } -> std::same_as<usize>;
    { a.reallocate(std::declval<T*>(), std::declval<usize>(), std::declval<u16>()) } -> std::same_as<ReallocReturn>;
    { a.reallocate_handle(std::declval<usize>(), std::declval<usize>(), std::declval<u16>()) } -> std::same_as<ReallocReturnHandle>;
    { a.free(std::declval<T*>()) } -> std::same_as<void>;
    { a.free_handle(std::declval<usize>()) } -> std::same_as<void>;
    { a.clear() } -> std::same_as<void>;
};

} // sf
