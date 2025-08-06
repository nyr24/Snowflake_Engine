#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_ds/iterator.hpp"
#include <initializer_list>
#include <utility>

namespace sf {

// _capacity and _len are in elements, not bytes
template<typename T, u32 Capacity>
struct FixedArray {
private:
    u32 _count;
    T   _buffer[Capacity];

public:
    using ValueType = T;
    using Pointer   = T*;

public:
    constexpr FixedArray() noexcept
        : _count{ 0 }
    {}

    explicit FixedArray(u32 count) noexcept
        : _count{ count }
    {
        SF_ASSERT_MSG(count <= Capacity, "Provided count should fit specified capacity");
    }

    FixedArray(std::initializer_list<T> init_list) noexcept
        : _count{ static_cast<u32>(init_list.size()) }
    {
        SF_ASSERT_MSG(init_list.size() <= Capacity, "Initializer list size don't fit for specified capacity");
        allocate(init_list.size());

        u32 i{0};
        for (auto curr = init_list.begin(); curr != init_list.end(); ++curr, ++i) {
            construct_at(_buffer + i, *curr);
        }
    }

    FixedArray(const FixedArray<T, Capacity>& rhs) noexcept
        : _count{ rhs._count }
    {
        sf_mem_copy((void*)(_buffer), (void*)(rhs._buffer), sizeof(T) * _count);
    }

    FixedArray<T, Capacity>& operator=(const FixedArray<T, Capacity>& rhs) noexcept {
        sf_mem_copy((void*)(_buffer), (void*)(rhs._buffer), sizeof(T) * _count);
        return *this;
    }

    FixedArray(FixedArray<T, Capacity>&& rhs) = delete;
    FixedArray<T, Capacity>& operator=(FixedArray<T, Capacity>&& rhs) = delete;

    friend bool operator==(const FixedArray<T, Capacity>& first, const FixedArray<T, Capacity>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        allocate_and_construct(std::forward<Args>(args)...);
    }

    void append(const T& item) noexcept {
        allocate_and_construct(item);
    }

    void remove_at(u32 index) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        if (index == _count - 1) {
            deallocate(1);
            return;
        }

        T* item = _buffer + index;

        if constexpr (std::is_destructible_v<T>) {
            item->~T();
        }

        sf_mem_move(item, item + 1, sizeof(T) * (_count - 1 - index));

        --_count;
    }

    // swaps element to the end and always removes it from the end, escaping memmove call
    void remove_unordered_at(u32 index) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        if (index == _count - 1) {
            deallocate(1);
            return;
        }

        T* item = _buffer + index;
        T* last = last_ptr();

        if constexpr (std::is_destructible_v<T>) {
            item->~T();
        }

        *item = *last;
        --_count;
    }

    constexpr void pop() noexcept {
        deallocate(1);
    }

    constexpr void clear() noexcept {
        deallocate(_count);
    }

    constexpr bool is_empty() const { return _count == 0; }
    constexpr bool is_full() const { return _count == Capacity; }

    constexpr T* data() noexcept { return _buffer; }
    constexpr T& first() noexcept { return *_buffer; }
    constexpr T* first_ptr() noexcept { return _buffer; }
    constexpr T* last_ptr() noexcept { return _buffer + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 capacity() const noexcept { return Capacity; }
    constexpr u32 capacity_remain() const noexcept { return Capacity - _count; }
    // const counterparts
    const T* data() const noexcept { return _buffer; }
    const T& first() const noexcept { return *_buffer; }
    const T& last() const noexcept { return *(_buffer + _count - 1); }
    const T* first_ptr() const noexcept { return _buffer; }
    const T* last_ptr() const noexcept { return _buffer + _count - 1; }

    constexpr PtrRandomAccessIterator<T> begin() const noexcept {
        return PtrRandomAccessIterator<T>(static_cast<const T*>(_buffer));
    }

    constexpr PtrRandomAccessIterator<T> end() const noexcept {
        return PtrRandomAccessIterator<T>(static_cast<const T*>(_buffer + _count));
    }

    T& operator[](u32 ind) noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return _buffer[ind];
    }

    const T& operator[](u32 ind) const noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return _buffer[ind];
    }

private:
    T* allocate(u32 alloc_count) noexcept
    {
        u32 free_capacity = Capacity - _count;

        if (free_capacity < alloc_count) {
            panic("Not enough memory");
        }

        T* return_memory = _buffer + _count;
        _count += alloc_count;

        return return_memory;
    }

    template<typename ...Args>
    void construct_at(T* ptr, Args&&... args) noexcept
    {
        sf_mem_place(ptr, args...);
    }

    void default_construct_at(T* ptr) noexcept
    {
        sf_mem_place(ptr);
    }

    template<typename ...Args>
    T* allocate_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = allocate(1);
        construct_at(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    T* allocate_and_default_construct(u32 count) noexcept
    {
        T* place_ptr = allocate(count);
        default_construct_at(place_ptr);
        return place_ptr;
    }

    void deallocate(u32 dealloc_count) noexcept
    {
        SF_ASSERT_MSG((dealloc_count) <= _count, "Can't deallocate more than all current elements");

        if constexpr (std::is_destructible_v<T>) {
            T* curr = last_ptr();

            for (u32 i{0}; i < dealloc_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= dealloc_count;
    }

}; // FixedArray

} // sf
