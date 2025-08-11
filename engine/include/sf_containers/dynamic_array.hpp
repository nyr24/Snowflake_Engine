#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/iterator.hpp"
#include <utility>

namespace sf {

// _capacity and _len are in elements, not bytes
template<typename T>
struct DynamicArray {
private:
    u32 _capacity;
    u32 _count;
    T*  _buffer;

public:
    using ValueType = T;
    using Pointer   = T*;

public:
    constexpr DynamicArray() noexcept
        : _capacity{ 0 }
        , _count{ 0 }
        , _buffer{ nullptr }
    {}

    explicit DynamicArray(u32 capacity) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {}

    explicit DynamicArray(u32 capacity, u32 count) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {
        SF_ASSERT_MSG(capacity >= count, "Count shouldn't be bigger than capacity");
        allocate_and_default_construct(count);
    }

    DynamicArray(std::pair<u32, std::initializer_list<T>> args) noexcept
        : _capacity{ static_cast<u32>(args.first) }
        , _count{0}
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {
        SF_ASSERT_MSG(args.second.size() <= _capacity, "Initializer list size don't fit for specified capacity");
        allocate(args.second.size());

        u32 i{0};
        for (auto curr = args.second.begin(); curr != args.second.end(); ++curr, ++i) {
            construct_at(_buffer + i, *curr);
        }
    }

    DynamicArray(std::initializer_list<T> init_list) noexcept
        : _capacity{ static_cast<u32>(init_list.size()) }
        , _count{0}
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {
        SF_ASSERT_MSG(init_list.size() <= _capacity, "Initializer list size don't fit for specified capacity");
        allocate(init_list.size());

        u32 i{0};
        for (auto curr = init_list.begin(); curr != init_list.end(); ++curr, ++i) {
            construct_at(_buffer + i, *curr);
        }
    }

    DynamicArray(DynamicArray<T>&& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ rhs._buffer }
    {
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._buffer = nullptr;
    }

    DynamicArray<T>& operator=(DynamicArray<T>&& rhs) noexcept
    {
        sf_mem_free_typed<T, true>(_buffer);
        _capacity = rhs._capacity;
        _count = rhs._count;
        _buffer = rhs._buffer;
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._buffer = nullptr;

        return *this;
    }

    DynamicArray(const DynamicArray<T>& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ sf_mem_alloc_typed<T, true>(rhs._capacity) }
    {
        sf_mem_copy(_buffer, rhs._buffer, rhs._count * sizeof(T));
        sf_mem_copy((void*)(_buffer), (void*)(rhs._buffer), rhs._count * sizeof(T));
    }

    DynamicArray<T>& operator=(const DynamicArray<T>& rhs) noexcept {
        if (_capacity < rhs._count) {
            sf_mem_free_typed<T, true>(_buffer);
            _buffer = sf_mem_alloc_typed<T, true>(rhs._capacity);
            _capacity = rhs._capacity;
        }
        _count = rhs._count;
        sf_mem_copy((void*)(_buffer), (void*)(rhs._buffer), rhs._count * sizeof(T));
    }

    ~DynamicArray() noexcept
    {
        if (_buffer) {
            sf_mem_free_typed<T, true>(_buffer);
            _buffer = nullptr;
        }
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

        if constexpr (std::is_move_assignable_v<T>) {
            *item = std::move(*last);
        } else {
            *item = *last;
        }

        --_count;
    }

    constexpr void pop() noexcept {
        deallocate(1);
    }

    constexpr void clear() noexcept {
        deallocate(_count);
    }

    constexpr void grow() noexcept {
        reallocate(_capacity * 2);
    }

    void reserve(u32 new_capacity) noexcept {
        reallocate(new_capacity);
    }

    void resize(u32 new_count) noexcept {
        if (new_count > _capacity) {
            reallocate(new_count);
        }
        if (new_count > _count) {
            allocate_and_default_construct(new_count - _count);
        }
    }

    void reallocate_and_resize(u32 new_capacity, u32 new_count) noexcept
    {
        SF_ASSERT_MSG(new_capacity >= new_count, "Invalid resize count");

        reallocate(new_capacity);
        if (new_count > _count) {
            allocate_and_default_construct(new_count - _count);
        }
    }

    constexpr bool is_empty() const { return _count == 0; }
    constexpr bool is_full() const { return _count == _capacity; }

    constexpr T* data() noexcept { return _buffer; }
    constexpr T& first() noexcept { return *_buffer; }
    constexpr T* first_ptr() noexcept { return _buffer; }
    constexpr T* last_ptr() noexcept { return _buffer + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 capacity() const noexcept { return _capacity; }
    constexpr u32 capacity_remain() const noexcept { return _capacity - _count; }
    // const counterparts
    const T* data() const noexcept { return _buffer; }
    const T& first() const noexcept { return *_buffer; }
    const T& last() const noexcept { return *(_buffer + _count - 1); }
    const T* first_ptr() const noexcept { return _buffer; }
    const T* last_ptr() const noexcept { return _buffer + _count - 1; }

    constexpr PtrRandomAccessIterator<T> begin() const noexcept {
        return PtrRandomAccessIterator<T>(_buffer);
    }

    constexpr PtrRandomAccessIterator<T> end() const noexcept {
        return PtrRandomAccessIterator<T>(_buffer + _count);
    }

    T& operator[](u32 ind) noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return _buffer[ind];
    }

    const T& operator[](u32 ind) const noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return _buffer[ind];
    }

    friend bool operator==(const DynamicArray<T>& first, const DynamicArray<T>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    static u64 hash(const DynamicArray<T>& key) noexcept {
        constexpr u64 PRIME = 1099511628211u;
        constexpr u64 OFFSET_BASIS = 14695981039346656037u;

        u64 hash = OFFSET_BASIS;

        for (u32 i{0}; i < key.count() * sizeof(T); ++i) {
            hash ^= reinterpret_cast<u8*>(key.data())[i];
            hash *= PRIME;
        }

        return hash;
    }

private:
    T* allocate(u32 alloc_count) noexcept
    {
        u32 free_capacity = _capacity - _count;

        if (free_capacity < alloc_count) {
            reallocate(_capacity * 2);
        }

        T* return_memory = _buffer + _count;
        _count += alloc_count;

        return return_memory;
    }

    void reallocate(u32 new_capacity) noexcept
    {
        if (new_capacity > _capacity) {
            T* new_buffer = sf_mem_realloc_typed<T>(_buffer, new_capacity);
            if (new_buffer != _buffer) {
                sf_mem_free_typed<T, true>(_buffer);
            }
            _capacity = new_capacity;
            _buffer = new_buffer;
        } else if (new_capacity < _capacity) {
            if (new_capacity < _count) {
                deallocate(_count - new_capacity);
            }
            _capacity = new_capacity;
        }
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
}; // DynamicArray

} // sf
