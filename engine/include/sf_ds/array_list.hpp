#pragma once

#include "sf_core/memory_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_ds/iterator.hpp"
#include <cassert>
#include <initializer_list>

namespace sf {

template<typename T, MonoTypeAllocator<T> A>
struct ArrayList {
public:
    using Iterator = RandomAccessIterator<T>;

    ArrayList(std::pair<std::initializer_list<T>, A>&& args);
    ArrayList(A&& allocator);
    // ArrayList(const ArrayList<T, A>& rhs);
    // ArrayList(ArrayList<T, A>&& rhs) noexcept;
    // ArrayList<T, A>& operator=(const ArrayList<T, A>& rhs);
    // ArrayList<T, A>& operator=(ArrayList<T, A>&& rhs) noexcept;
    // ~ArrayList();

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept;
    template<typename ...Args>
    void append(const T& item) noexcept;
    template<typename ...Args>
    void append(T&& item) noexcept;
    void pop() noexcept;
    void debug_print() noexcept;
    usize len() noexcept;
    usize capacity() noexcept;
    T& operator[](usize ind) noexcept;
    const T& operator[](usize ind) const noexcept;
    RandomAccessIterator<T> begin() noexcept;
    RandomAccessIterator<T> end() noexcept;
private:
    A allocator;
};

template<typename T, MonoTypeAllocator<T> A>
ArrayList<T, A>::ArrayList(std::pair<std::initializer_list<T>, A>&& args)
    : allocator{ std::move(args.second) }
{
    allocator.allocate(args.first.size());

    u16 i{0};
    auto iter{args.first.begin()};

    for (; iter != args.first.end(); ++iter, ++i) {
        allocator.construct(
            allocator.ptr_offset(i),
            *iter
        );
    }

    debug_print();
}

template<typename T, MonoTypeAllocator<T> A>
ArrayList<T, A>::ArrayList(A&& allocator)
    : allocator{ std::move(allocator) }
{}

template<typename T, MonoTypeAllocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append_emplace(Args&&... args) noexcept {
    allocator.allocate_and_construct(std::forward<Args>(args)...);
}

template<typename T, MonoTypeAllocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append(const T& item) noexcept {
    allocator.allocate_and_construct(item);
}

template<typename T, MonoTypeAllocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append(T&& item) noexcept {
    allocator.allocate_and_construct(item);
}

template<typename T, MonoTypeAllocator<T> A>
void ArrayList<T, A>::pop() noexcept {
    if (allocator.len() > 0) {
        allocator.pop();
    }
}

template<typename T, MonoTypeAllocator<T> A>
void ArrayList<T, A>::debug_print() noexcept {
    if constexpr (HasFormatter<T, u8>) {
        for (u32 i{0}; i < allocator.len(); ++i) {
            LOG_DEBUG("{} ", allocator.ptr_offset_val(i));
        }
    }
}

template<typename T, MonoTypeAllocator<T> A>
T& ArrayList<T, A>::operator[](usize ind) noexcept {
    SF_ASSERT_MSG((ind >= 0 && ind < allocator.len()), "out of bounds");
    return allocator.ptr_offset_val(ind);
}

template<typename T, MonoTypeAllocator<T> A>
const T& ArrayList<T, A>::operator[](usize ind) const noexcept {
    SF_ASSERT_MSG((ind >= 0 && ind < allocator.len()), "out of bounds");
    return allocator.ptr_offset_val(ind);
}

template<typename T, MonoTypeAllocator<T> A>
u64 ArrayList<T, A>::len() noexcept {
    return allocator.len();
}

template<typename T, MonoTypeAllocator<T> A>
u64 ArrayList<T, A>::capacity() noexcept {
    return allocator.capacity();
}

template<typename T, MonoTypeAllocator<T> A>
RandomAccessIterator<T> ArrayList<T, A>::begin() noexcept {
    return RandomAccessIterator<T>(allocator.begin());
}

template<typename T, MonoTypeAllocator<T> A>
RandomAccessIterator<T> ArrayList<T, A>::end() noexcept {
    return RandomAccessIterator<T>(allocator.end());
}

} // sf
