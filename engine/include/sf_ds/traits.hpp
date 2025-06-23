#pragma once

#include "sf_core/types.hpp"
#include <concepts>
#include <utility>

namespace sf {

template<typename A, typename T, typename ...Args>
concept MonoTypeAllocatorTrait = requires(A a, Args... args) {
    std::movable<A>;
    std::copyable<A>;

    { a.allocate(std::declval<u64>()) } -> std::same_as<T*>;
    { a.deallocate(std::declval<T*>(), std::declval<u64>()) } -> std::same_as<void>;
    { a.construct(std::declval<T*>(), std::forward<Args>(args)...) } -> std::same_as<void>;
    { a.allocate_and_construct(std::forward<Args>(args)...) } -> std::same_as<T*>;
    { a.reallocate(std::declval<u64>()) } -> std::same_as<void>;
    { a.begin() } -> std::same_as<T*>;
    { a.end() } -> std::same_as<T*>;
    { a.ptr_offset(std::declval<u64>()) } -> std::same_as<T*>;
    { a.ptr_offset_val(std::declval<u64>()) } -> std::same_as<T&>;
    { a.len() } -> std::same_as<usize>;
    { a.capacity() } -> std::same_as<usize>;
    { a.pop() } -> std::same_as<void>;
    { a.clear() } -> std::same_as<void>;
};

// TODO:
template<typename A, typename ...Args>
concept PolyTypeAllocatorTrait = true;

template<typename MaybeIter, typename T>
concept IteratorTrait = requires (MaybeIter iter, MaybeIter iter2) {
    typename MaybeIter::ValueType;
    typename MaybeIter::Ptr;
    typename MaybeIter::LvalueRef;
    typename MaybeIter::RvalueRef;
    typename MaybeIter::ConstRef;
    typename MaybeIter::Diff;
    typename MaybeIter::Strategy;

    std::copyable<MaybeIter>;
    std::destructible<MaybeIter>;
    std::totally_ordered<MaybeIter>;
    std::default_initializable<MaybeIter>;

    { *iter } -> std::same_as<T&>;
    { iter += 1 } -> std::same_as<MaybeIter&>;
    { iter -= 1 } -> std::same_as<MaybeIter&>;
    { iter + iter2 } -> std::same_as<MaybeIter>;
    { iter - iter2 } -> std::same_as<MaybeIter>;
    { iter++ } -> std::same_as<MaybeIter>;
    { ++iter } -> std::same_as<MaybeIter&>;
    { iter-- } -> std::same_as<MaybeIter>;
    { --iter } -> std::same_as<MaybeIter&>;
    { iter[1] } -> std::same_as<T&>;
};

} // sf
