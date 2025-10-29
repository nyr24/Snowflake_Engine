#pragma once

#include "sf_core/defines.hpp"
#include "sf_containers/optional.hpp"
#include <concepts>
#include <utility>

namespace sf {

template<typename A, typename T = void, typename ...Args>
concept AllocatorTrait = requires(A a, Args... args) {
    { a.allocate(std::declval<u32>(), std::declval<u16>()) } -> std::same_as<T*>;
    { a.allocate_handle(std::declval<u32>(), std::declval<u16>()) } -> std::same_as<usize>;
    { a.get_mem_with_handle(std::declval<usize>()) } -> std::same_as<T*>;
    { a.reallocate(std::declval<T*>(), std::declval<u32>(), std::declval<u16>()) } -> std::same_as<T*>;
    { a.reallocate_handle(std::declval<usize>(), std::declval<u32>(), std::declval<u16>()) } -> std::same_as<Option<usize>>;
    { a.free(std::declval<T*>()) } -> std::same_as<void>;
    { a.free_handle(std::declval<usize>()) } -> std::same_as<void>;
    { a.clear() } -> std::same_as<void>;
};

} // sf
