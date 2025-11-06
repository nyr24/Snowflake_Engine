#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/iterator.hpp"
#include <span>
#include <type_traits>
#include <utility>

namespace sf {

// _capacity and _len are in elements, not bytes
template<typename T, f32 GROW_FACTOR = 2.0f, u32 DEFAULT_CAPACITY = 8>
struct DynamicArraySelfManaged {
private:
    u32 _capacity;
    u32 _count;
    T*  _buffer;

public:
    using ValueType     = T;
    using PointerType   = T*;

public:
    constexpr DynamicArraySelfManaged() noexcept
        : _capacity{ 0 }
        , _count{ 0 }
        , _buffer{ nullptr }
    {}

    explicit DynamicArraySelfManaged(u32 capacity) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ static_cast<T*>(sf_mem_alloc(capacity * sizeof(T), alignof(T))) }
    {}

    explicit DynamicArraySelfManaged(u32 capacity, u32 count) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ static_cast<T*>(sf_mem_alloc(capacity * sizeof(T), alignof(T))) }
    {
        SF_ASSERT_MSG(capacity >= count, "Count shouldn't be bigger than capacity");

        if constexpr (std::is_trivial_v<T>) {
            move_forward(count);
        } else {
            move_forward_and_default_construct(count);
        }
    }

    DynamicArraySelfManaged(std::pair<u32, std::initializer_list<T>> args) noexcept
        : _capacity{ static_cast<u32>(args.first) }
        , _count{0}
        , _buffer{ static_cast<T*>(sf_mem_alloc(_capacity * sizeof(T), alignof(T))) }
    {
        SF_ASSERT_MSG(args.second.size() <= _capacity, "Initializer list size don't fit for specified capacity");
        move_forward(args.second.size());

        u32 i{0};
        for (auto curr = args.second.begin(); curr != args.second.end(); ++curr, ++i) {
            if constexpr (std::is_trivial_v<T>) {
                _buffer[i] = *curr;
            } else {
                construct_at(_buffer + i, *curr);
            }
        }
    }

    DynamicArraySelfManaged(std::initializer_list<T> init_list) noexcept
        : _capacity{ static_cast<u32>(init_list.size()) }
        , _count{0}
        , _buffer{ sf_mem_alloc(_capacity * sizeof(T)) }
    {
        SF_ASSERT_MSG(init_list.size() <= _capacity, "Initializer list size don't fit for specified capacity");
        move_forward(init_list.size());

        u32 i{0};
        for (auto curr = init_list.begin(); curr != init_list.end(); ++curr, ++i) {
            if constexpr (std::is_trivial_v<T>) {
                _buffer[i] = *curr;
            } else {
                construct_at(_buffer + i, *curr);
            }
        }
    }

    DynamicArraySelfManaged(DynamicArraySelfManaged<T>&& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ rhs._buffer }
    {
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._buffer = nullptr;
    }

    DynamicArraySelfManaged<T>& operator=(DynamicArraySelfManaged<T>&& rhs) noexcept
    {
        if (this == &rhs) return *this;

        sf_mem_free(_buffer, alignof(T));
        _capacity = rhs._capacity;
        _count = rhs._count;
        _buffer = rhs._buffer;
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._buffer = nullptr;

        return *this;
    }

    DynamicArraySelfManaged(const DynamicArraySelfManaged<T>& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ sf_mem_alloc_typed<T, true>(rhs._capacity) }
    {
        sf_mem_copy((void*)_buffer, (void*)rhs._buffer, rhs._count * sizeof(T));
    }

    DynamicArraySelfManaged<T>& operator=(const DynamicArraySelfManaged<T>& rhs) noexcept {
        if (this == &rhs) return *this;

        if (_capacity < rhs._capacity) {
            grow(rhs._capacity);
        }
        _count = rhs._count;
        sf_mem_copy((void*)_buffer, (void*)rhs._buffer, rhs._count * sizeof(T));

        return *this;
    }

    ~DynamicArraySelfManaged() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, alignof(T));
            _buffer = nullptr;
        }
    }

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        allocate_and_construct(std::forward<Args>(args)...);
    }

    void append(const T& item) noexcept {
        if constexpr (std::is_trivial_v<T>) {
            move_forward(1);
            _buffer[_count - 1] = item;
        } else {
            move_forward_and_construct(std::move(item));
        }
    }

    void append(T&& item) noexcept {
        if constexpr (std::is_trivial_v<T>) {
            move_forward(1);
            _buffer[_count - 1] = item;
        } else {
            move_forward_and_construct(std::move(item));
        }
    }

    void remove_at(u32 index) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        if (index == _count - 1) {
            move_ptr_backwards(1);
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
            move_ptr_backwards(1);
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
        SF_ASSERT(_count > 0);
        move_ptr_backwards(1);
    }

    constexpr void pop_range(u32 count) noexcept {
        SF_ASSERT(count <= _count);
        move_ptr_backwards(count);
    }

    constexpr void clear() noexcept {
        move_ptr_backwards(_count);
    }

    void reserve(u32 new_capacity) noexcept {
        if (new_capacity > _capacity) {
            grow(new_capacity);
        }
    }

    void resize(u32 new_count) noexcept {
        if (new_count > _capacity) {
            grow(new_count);
        }
        if (new_count > _count) {
            if constexpr (std::is_trivial_v<T>) {
                move_forward(new_count - _count);
            } else {
                move_forward_and_default_construct(new_count - _count);
            }
        }
    }

    void reserve_and_resize(u32 new_capacity, u32 new_count) noexcept
    {
        SF_ASSERT_MSG(new_capacity >= new_count, "Invalid resize count");

        if (new_capacity > _capacity) {
            grow(new_capacity);
        }

        if (new_count > _count) {
            if constexpr (std::is_trivial_v<T>) {
                move_forward(new_count - _count);
            } else {
                move_forward_and_default_construct(new_count - _count);
            }
        }
    }

    constexpr bool is_empty() const { return _count == 0; }
    constexpr bool is_full() const { return _count == _capacity; }

    std::span<T> to_span(u32 start = 0, u32 len = 0) noexcept {
        return std::span{ _buffer + start, len == 0 ? _count : len };
    }

    std::span<const T> to_span(u32 start = 0, u32 len = 0) const noexcept {
        return std::span{ _buffer + start, len == 0 ? _count : len };
    }

    constexpr T* data() noexcept { return _buffer; }
    constexpr T& first() noexcept { return *_buffer; }
    constexpr T* first_ptr() noexcept { return _buffer; }
    constexpr T* last_ptr() noexcept { return _buffer + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 size_in_bytes() const noexcept { return sizeof(T) * _count; }
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

    friend bool operator==(const DynamicArraySelfManaged<T>& first, const DynamicArraySelfManaged<T>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    static u64 hash(const DynamicArraySelfManaged<T>& key) noexcept {
        constexpr u64 PRIME = 1099511628211u;
        constexpr u64 OFFSET_BASIS = 14695981039346656037u;

        u64 hash = OFFSET_BASIS;

        for (u32 i{0}; i < key.count() * sizeof(T); ++i) {
            hash ^= reinterpret_cast<u8*>(key.data())[i];
            hash *= PRIME;
        }

        return hash;
    }

    void grow(u32 new_capacity) noexcept
    {
        if (_capacity == 0) {
            _capacity = DEFAULT_CAPACITY;
        }
        while (_capacity < new_capacity) {
            _capacity *= GROW_FACTOR;
        }
        T* new_buffer = static_cast<T*>(sf_mem_realloc(_buffer, _capacity));
        _buffer = new_buffer;
    }

    void shrink(u32 new_capacity) noexcept
    {
        if (new_capacity < _count) {
            move_ptr_backwards(_count - new_capacity);
            _count = new_capacity;
        }
        _capacity = new_capacity;
    }
private:
    T* move_forward_and_get_ptr(u32 alloc_count) noexcept
    {
        u32 free_capacity = _capacity - _count;
        if (free_capacity < alloc_count) {
            grow(_capacity * 2);
        }
        T* return_memory = _buffer + _count;
        _count += alloc_count;
        return return_memory;
    }

    void move_forward(u32 alloc_count) noexcept
    {
        u32 free_capacity = _capacity - _count;
        if (free_capacity < alloc_count) {
            grow(_capacity * GROW_FACTOR);
        }
        _count += alloc_count;
    }

    void move_ptr_backwards(u32 dealloc_count) noexcept
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
    T* move_forward_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = move_forward_and_get_ptr(1);
        construct_at(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    T* move_forward_and_default_construct(u32 count) noexcept
    {
        T* place_ptr = move_forward_and_get_ptr(count);
        default_construct_at(place_ptr);
        return place_ptr;
    }

}; // DynamicArraySelfManaged

using StringSelfManaged = DynamicArraySelfManaged<char>;

} // sf
