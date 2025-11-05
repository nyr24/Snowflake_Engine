#pragma once

#include "sf_containers/optional.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/iterator.hpp"
#include <initializer_list>
#include <span>
#include <string_view>
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

    constexpr explicit FixedArray(u32 count) noexcept
        : _count{ count }
    {
        SF_ASSERT_MSG(count <= Capacity, "Provided count should fit specified capacity");
    }

    constexpr FixedArray(std::initializer_list<T> init_list) noexcept
        : _count{ 0 }
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
        sf_mem_copy((void*)_buffer, (void*)(rhs._buffer), sizeof(T) * _count);
    }

    FixedArray<T, Capacity>& operator=(const FixedArray<T, Capacity>& rhs) noexcept {
        _count = rhs._count;
        sf_mem_copy((void*)_buffer, (void*)rhs._buffer, sizeof(T) * _count);
        return *this;
    }

    FixedArray(std::string_view str) noexcept
        : _count{static_cast<u32>(str.size())}
    {
        sf_mem_copy((void*)_buffer, (void*)str.data(), sizeof(T) * _count);
    }

    FixedArray(std::span<T> sp) noexcept
        : _count{ sp.size() }
    {
        SF_ASSERT_MSG(sp.size() <= Capacity, "Should not exceed capacity");
        sf_mem_copy((void*)_buffer, (void*)sp.data(), sizeof(T) * _count); 
    }

    constexpr FixedArray(FixedArray<T, Capacity>&& rhs) noexcept
        : _count{ rhs._count }
    {
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);
    }

    constexpr FixedArray<T, Capacity>& operator=(FixedArray<T, Capacity>&& rhs) noexcept
    {
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);
        return *this;
    }

    constexpr FixedArray<T, Capacity>& operator=(const std::span<T> sp) noexcept
    {
        SF_ASSERT_MSG(sp.size() <= Capacity, "Should not exceed capacity");
        _count = sp.size();
        sf_mem_copy((void*)_buffer, (void*)sp.data(), sp.size()); 
        return *this;
    }

    constexpr FixedArray<T, Capacity>& operator=(const std::span<const T> sp) noexcept
    {
        SF_ASSERT_MSG(sp.size() <= Capacity, "Should not exceed capacity");
        _count = sp.size();
        sf_mem_copy((void*)_buffer, (void*)sp.data(), sp.size()); 
        return *this;
    }

    friend bool operator==(const FixedArray<T, Capacity>& first, const FixedArray<T, Capacity>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    friend bool operator==(const FixedArray<T, Capacity>& arr, std::string_view sv) noexcept {
        if (arr.count() != sv.length()) {
            return false;
        }
        
        return sf_mem_cmp(arr.data(), sv.data(), arr.count());
    }

    template<typename ...Args>
    constexpr void append_emplace(Args&&... args) noexcept {
        allocate_and_construct(std::forward<Args>(args)...);
    }

    constexpr void append(const T& item) noexcept {
        allocate_and_construct(item);
    }

    constexpr void append(T&& item) noexcept {
        allocate_and_construct(std::move(item));
    }

    constexpr void append(std::span<T> sp) noexcept {
        allocate(sp.size());
        sf_mem_copy(_buffer + _count, sp.data(), sp.size());
    }

    constexpr void remove_at(u32 index) noexcept {
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
    constexpr void remove_unordered_at(u32 index) noexcept {
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

    constexpr void resize(u32 count) noexcept {
        if (count <= Capacity) {
            _count = count;
        }
    }

    constexpr void resize_to_capacity() noexcept {
        _count = Capacity;
    }

    constexpr void pop() noexcept {
        deallocate(1);
    }

    constexpr void pop_range(u32 count) noexcept {
        SF_ASSERT_MSG(count <= _count, "Can't pop more than have");
        deallocate(count);
    }

    constexpr void clear() noexcept {
        deallocate(_count);
    }

    constexpr void fill(ConstLRefOrValType<T> val) noexcept {
        for (u32 i{0}; i < Capacity; ++i) {
            _buffer[i] = val;
        }
        _count = Capacity;
    }

    constexpr bool cmp_ranges(const FixedArray<T, Capacity>& rhs, u32 start, u32 rhs_start, u32 count) const noexcept {
        return sf_mem_cmp(this->data_offset(start), rhs.data_offset(rhs_start), count);
    }

    constexpr std::span<T> to_span(u32 start = 0, u32 len = 0) noexcept {
        return std::span{ _buffer + start, len == 0 ? _count : len };
    }

    constexpr std::span<const T> to_span(u32 start = 0, u32 len = 0) const noexcept {
        return std::span{ _buffer + start, len == 0 ? _count : len };
    }

    constexpr std::string_view to_string_view(u32 start = 0, u32 len = 0) noexcept {
        return std::string_view{ _buffer + start, len == 0 ? _count : len };
    }

    constexpr std::string_view to_string_view(u32 start = 0, u32 len = 0) const noexcept {
        return std::string_view{ static_cast<const T*>(_buffer + start), len == 0 ? _count : len };
    }

    constexpr bool has(ConstLRefOrValType<T> item) noexcept {
        for (u32 i{0}; i < _count; ++i) {
            if (_buffer[i] == item) {
                return true;
            }
        }

        return false;
    }

    Option<u32> index_of(ConstLRefOrValType<T> item) noexcept {
        for (u32 i{0}; i < _count; ++i) {
            if (_buffer[i] == item) {
                return i;
            }
        }

        return {None::VALUE};
    }

    constexpr bool is_empty() const noexcept { return _count == 0; }
    constexpr bool is_full() const noexcept { return _count == Capacity; }
    constexpr T* data() noexcept { return _buffer; }
    constexpr T* data_offset(u32 i) noexcept { return _buffer + i; }
    constexpr T& first() noexcept { return *_buffer; }
    constexpr T& last() noexcept { return *(_buffer + _count - 1); }
    constexpr T& last_past_end() noexcept { return *(_buffer + _count); }
    constexpr T* first_ptr() noexcept { return _buffer; }
    constexpr T* last_ptr() noexcept { return _buffer + _count - 1; }
    // const counterparts
    const T* data() const noexcept { return _buffer; }
    const T* data_offset(u32 i) const noexcept { return _buffer + i; }
    const T& first() const noexcept { return *_buffer; }
    const T& last() const noexcept { return *(_buffer + _count - 1); }
    const T& last_past_end() const noexcept { return *(_buffer + _count); }
    const T* first_ptr() const noexcept { return _buffer; }
    const T* last_ptr() const noexcept { return _buffer + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 size_in_bytes() const noexcept { return sizeof(T) * Capacity; }
    constexpr u32 capacity() const noexcept { return Capacity; }
    constexpr u32 capacity_remain() const noexcept { return Capacity - _count; }

    constexpr PtrRandomAccessIterator<T> begin() const noexcept {
        return PtrRandomAccessIterator<T>(static_cast<const T*>(_buffer));
    }

    constexpr PtrRandomAccessIterator<T> end() const noexcept {
        return PtrRandomAccessIterator<T>(static_cast<const T*>(_buffer + _count));
    }

    constexpr PtrRandomAccessIterator<T> begin() noexcept {
        return PtrRandomAccessIterator<T>(static_cast<T*>(_buffer));
    }

    constexpr PtrRandomAccessIterator<T> end() noexcept {
        return PtrRandomAccessIterator<T>(static_cast<T*>(_buffer + _count));
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
    constexpr T* allocate(u32 alloc_count) noexcept
    {
        u32 free_capacity = Capacity - _count;

        if (free_capacity < alloc_count) {
            panic("Not enough memory in array buffer");
        }

        T* return_memory = _buffer + _count;
        _count += alloc_count;

        return return_memory;
    }

    template<typename ...Args>
    constexpr void construct_at(T* ptr, Args&&... args) noexcept
    {
        sf_mem_place(ptr, args...);
    }

    constexpr void default_construct_at(T* ptr) noexcept
    {
        sf_mem_place(ptr);
    }

    template<typename ...Args>
    constexpr void allocate_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = allocate(1);
        construct_at(place_ptr, std::forward<Args>(args)...);
    }

    constexpr void allocate_and_default_construct(u32 count) noexcept
    {
        T* place_ptr = allocate(count);
        default_construct_at(place_ptr);
    }

    constexpr void deallocate(u32 dealloc_count) noexcept
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
