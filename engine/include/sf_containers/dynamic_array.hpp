#pragma once

#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/iterator.hpp"
#include "sf_core/utility.hpp"
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sf {

// _capacity and _len are in elements, not bytes
template<typename T, AllocatorTrait Allocator = GeneralPurposeAllocator, u64 MAX_ITEMS = 0>
struct DynamicArray {    
private:
    Allocator* _allocator;
    usize      _data;
    u32        _capacity;
    u32        _count;

public:
    using ValueType = T;
    using Pointer   = T*;

public:
    DynamicArray() noexcept
        : _allocator{nullptr}
        , _data{INVALID_ALLOC_HANDLE}
        , _capacity{0}
        , _count{0}
    {}

    explicit DynamicArray(Allocator* allocator) noexcept
        : _allocator{allocator}
        , _data{INVALID_ALLOC_HANDLE}
        , _capacity{0}
        , _count{0}
    {}

    explicit DynamicArray(u32 capacity_input, Allocator* allocator) noexcept
        : _allocator{ allocator }
        , _data{ INVALID_ALLOC_HANDLE }
        , _capacity{ capacity_input }
        , _count{ 0 }
    {
        if constexpr (MAX_ITEMS != 0) {
            SF_ASSERT_MSG(_capacity <= MAX_ITEMS, "Should not exceed max item limit");
        }
        if (_allocator) {
            _data = allocator->allocate_handle(capacity_input * sizeof(T), alignof(T));
        } else {
            LOG_ERROR("DynamicArray: provided allocator is nullptr");
        }
    }

    explicit DynamicArray(u32 capacity_input, u32 count, Allocator* allocator) noexcept
        : _allocator{ allocator }
        , _data{ INVALID_ALLOC_HANDLE }
        , _capacity{ capacity_input }
        , _count{ 0 }
    {
        if constexpr (MAX_ITEMS != 0) {
            SF_ASSERT_MSG(_capacity <= MAX_ITEMS, "Should not exceed max item limit");
        }
        SF_ASSERT_MSG(capacity_input >= count, "Count shouldn't be bigger than capacity");

        if (_allocator) {
            _data = allocator->allocate_handle(capacity_input * sizeof(T), alignof(T));
            move_ptr_forward(count);
        } else {
            LOG_ERROR("DynamicArray: provided allocator is nullptr");
        }
    }

    DynamicArray(std::tuple<Allocator*, u32, std::initializer_list<T>>&& config) noexcept
        : _allocator{ std::get<0>(config) }
        , _data{ INVALID_ALLOC_HANDLE }
        , _capacity{ static_cast<u32>(std::get<1>(config)) }
        , _count{0}
    {
        if constexpr (MAX_ITEMS != 0) {
            SF_ASSERT_MSG(_capacity <= MAX_ITEMS, "Should not exceed max item limit");
        }

        auto& init_list{ std::get<2>(config) };
        SF_ASSERT_MSG(init_list.size() <= _capacity, "Initializer list size don't fit for specified capacity");

        if (_allocator) {
            _data = _allocator->allocate_handle(std::get<1>(config) * sizeof(T), alignof(T)) ;
            move_ptr_forward(init_list.size());
            u32 i{0};
            for (auto curr = init_list.begin(); curr != init_list.end(); ++curr, ++i) {
                construct_at(_data + i, *curr);
            }
        } else {
            LOG_ERROR("DynamicArray: provided allocator is nullptr");
        }
    }

    DynamicArray(std::tuple<Allocator*, std::initializer_list<T>>&& config) noexcept
        : _allocator{ std::get<0>(config) }
        , _data{ INVALID_ALLOC_HANDLE }
        , _capacity{ static_cast<u32>(std::get<1>(config).size()) }
        , _count{0}
    {
        if constexpr (MAX_ITEMS != 0) {
            SF_ASSERT_MSG(_capacity <= MAX_ITEMS, "Should not exceed max item limit");
        }
        if (_allocator) {
            _data = _allocator->allocate_handle(std::get<1>(config).size() * sizeof(T), alignof(T));
            auto& init_list{ std::get<1>(config) };
            move_ptr_forward(init_list.size());

            u32 i{0};
            for (auto curr = init_list.begin(); curr != init_list.end(); ++curr, ++i) {
                construct_at(handle_to_ptr() + i, *curr);
            }
        } else {
            LOG_ERROR("DynamicArray: provided allocator is nullptr");
        }
    }

    DynamicArray(DynamicArray<T, Allocator>&& rhs) noexcept
        : _allocator{ rhs._allocator }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _data{ rhs._data }
    {
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._data = INVALID_ALLOC_HANDLE;
    }

    DynamicArray<T, Allocator>& operator=(DynamicArray<T, Allocator>&& rhs) noexcept
    {
        if (this == &rhs) return *this;

        if (_allocator && _data != INVALID_ALLOC_HANDLE) {
            _allocator->free_handle(_data);
        }
        _allocator = rhs._allocator;
        _capacity = rhs._capacity;
        _count = rhs._count;
        _data = rhs._data;
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._data = INVALID_ALLOC_HANDLE;

        return *this;
    }

    DynamicArray(const DynamicArray<T, Allocator>& rhs) noexcept
        : _allocator{ rhs._allocator }
        , _data{ INVALID_ALLOC_HANDLE }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
    {
        if (rhs._allocator && rhs._count > 0) {
            _allocator->allocate_handle(rhs._capacity * sizeof(T), alignof(T));
            sf_mem_copy((void*)_data, (void*)rhs._data, rhs._count * sizeof(T));
        }
    }

    DynamicArray<T, Allocator>& operator=(const DynamicArray<T, Allocator>& rhs) noexcept {
        if (this == &rhs) return *this;

        if (_allocator && _data != INVALID_ALLOC_HANDLE) {
            _allocator->free_handle(_data);
        }

        _allocator = rhs._allocator;

        if (_capacity < rhs._capacity) {
            resize_internal(rhs._capacity);
        }

        if (rhs._count > 0) {
            _count = rhs._count;
            sf_mem_copy((void*)_data, (void*)rhs._data, rhs._count * sizeof(T));
        }

        return *this;
    }

    ~DynamicArray() noexcept
    {
        free();
    }

    void free() noexcept {
        if (_allocator && _data != INVALID_ALLOC_HANDLE) {
            _allocator->free_handle(_data);
            _data = INVALID_ALLOC_HANDLE;
        }
    }

    bool is_free() noexcept {
        return _data = INVALID_ALLOC_HANDLE;
    }

    void set_allocator(Allocator* allocator) noexcept
    {
        if (allocator) {
            _allocator = allocator;
        } else {
            LOG_ERROR("DynamicArray : set_allocator(): provided allocator is nullptr");
        }
    }
    
    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        if constexpr (MAX_ITEMS != 0) {
            if (_count + 1 > MAX_ITEMS) {
                LOG_ERROR("Maximum limit of items exceeded");
                return;
            }
        }
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");
        
        move_ptr_and_construct(std::forward<Args>(args)...);
    }

    void append(const T& item) noexcept {
        if constexpr (MAX_ITEMS != 0) {
            if (_count + 1 > MAX_ITEMS) {
                LOG_ERROR("Maximum limit of items exceeded");
                return;
            }
        }

        move_ptr_and_construct(item);
    }

    void append(T&& item) noexcept {
        if constexpr (MAX_ITEMS != 0) {
            if (_count + 1 > MAX_ITEMS) {
                LOG_ERROR("Maximum limit of items exceeded");
                return;
            }
        }

        move_ptr_and_construct(std::move(item));
    }

    void remove_at(u32 index) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        if (index == _count - 1) {
            move_ptr_backwards(1);
            return;
        }

        T* item = handle_to_ptr() + index;

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

        T* item = handle_to_ptr() + index;
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
        move_ptr_backwards(1);
    }

    constexpr void clear() noexcept {
        move_ptr_backwards(_count);
    }

    constexpr void fill(ConstLRefOrValType<T> val) noexcept {
        for (u32 i{0}; i < _capacity; ++i) {
            handle_to_ptr()[i] = val;
        }
    }

    constexpr void grow() noexcept {
        SF_ASSERT_MSG(_allocator, "Allocator should be set");
        resize_internal(_capacity * 2);
    }

    void reserve(u32 new_capacity) noexcept {
        if constexpr (MAX_ITEMS != 0) {
            if (new_capacity > MAX_ITEMS) {
                LOG_ERROR("Can't reserve exactly with input value, Maximum limit of items will be exceeded");
                new_capacity = MAX_ITEMS;
            }
        }

        if (!_allocator) {
            LOG_ERROR("DynamicArray : reserve(): _allocator should be valid pointer");
            return;
        }

        if (new_capacity > _capacity) {
            resize_internal(new_capacity);
        }
    }

    void resize(u32 new_count) noexcept {
        if constexpr (MAX_ITEMS != 0) {
            if (new_count > MAX_ITEMS) {
                LOG_ERROR("Can't resize exactly with input value, Maximum limit of items will be exceeded");
                new_count = MAX_ITEMS;
            }
        }

        if (!_allocator) {
            LOG_ERROR("DynamicArray : resize(): _allocator should be valid pointer");
            return;
        }

        if (new_count > _capacity) {
            resize_internal(new_count);
        }
        if (new_count > _count) {
            move_ptr_and_default_construct(new_count - _count);
        }
    }

    void reserve_and_resize(u32 new_capacity, u32 new_count) noexcept
    {
        SF_ASSERT_MSG(new_capacity >= new_count, "Invalid resize count");

        if (!_allocator) {
            LOG_ERROR("DynamicArray : reserve_and_resize(): _allocator should be valid pointer");
            return;
        }

        if (new_capacity > _capacity) {
            resize_internal(new_capacity);
        }

        if (new_count > _count) {
            move_ptr_and_default_construct(new_count - _count);
        }
    }

    constexpr bool is_empty() const { return _count == 0; }
    constexpr bool is_full() const { return _count == _capacity; }
    constexpr T* data() noexcept { return handle_to_ptr(); }
    constexpr T& first() noexcept { return *(handle_to_ptr()); }
    constexpr T& last() noexcept { return *(handle_to_ptr() + _count - 1); }
    constexpr T& last_past_end() noexcept { return *(handle_to_ptr() + _count); }
    constexpr T* first_ptr() noexcept { return handle_to_ptr(); }
    constexpr T* last_ptr() noexcept { return handle_to_ptr() + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 size_in_bytes() const noexcept { return sizeof(T) * _count; }
    constexpr u32 capacity() const noexcept { return _capacity; }
    constexpr u32 capacity_remain() const noexcept { return _capacity - _count; }
    // const counterparts
    const T* data() const noexcept { return handle_to_ptr(); }
    const T& first() const noexcept { return *(handle_to_ptr()); }
    const T& last() const noexcept { return *(handle_to_ptr() + _count - 1); }
    const T& last_past_end() const noexcept { return *(handle_to_ptr() + _count); }
    const T* first_ptr() const noexcept { return handle_to_ptr(); }
    const T* last_ptr() const noexcept { return handle_to_ptr() + _count - 1; }

    constexpr PtrRandomAccessIterator<T> begin() const noexcept {
        return PtrRandomAccessIterator<T>(handle_to_ptr());
    }

    constexpr PtrRandomAccessIterator<T> end() const noexcept {
        return PtrRandomAccessIterator<T>(handle_to_ptr() + _count);
    }

    T& operator[](u32 ind) noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return (handle_to_ptr())[ind];
    }

    const T& operator[](u32 ind) const noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return (handle_to_ptr())[ind];
    }

    friend bool operator==(const DynamicArray<T, Allocator, MAX_ITEMS>& first, const DynamicArray<T, Allocator, MAX_ITEMS>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._allocator->get_mem_with_handle(first._data), second._allocator->get_mem_with_handle(second._data), first._count * sizeof(T));
    }

    static u64 hash(const DynamicArray<T, Allocator>& key) noexcept {
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
    constexpr T* handle_to_ptr() const {
        return static_cast<T*>(_allocator->get_mem_with_handle(_data));
    }

    T* move_ptr_forward(u32 alloc_count) noexcept
    {
        u32 free_capacity = _capacity - _count;

        if (free_capacity < alloc_count) {
            if (!resize_internal(_capacity * 2)) {
                return nullptr;
            }
        }

        T* return_memory = handle_to_ptr() + _count;
        _count += alloc_count;

        return return_memory;
    }

    bool resize_internal(u32 new_capacity) noexcept
    {
        if (!_allocator) {
            LOG_ERROR("DynamicArray : resize_internal(): _allocator should be valid pointer");
            return false;
        }

        if (_data == INVALID_ALLOC_HANDLE) {
            _data = _allocator->allocate_handle(new_capacity, alignof(T));
            _capacity = new_capacity;
        }
        
        if (new_capacity > _capacity) {
            Option<usize> maybe_handle = _allocator->reallocate_handle(_data, new_capacity * sizeof(T), alignof(T));
            if (maybe_handle.is_none()) {
                LOG_ERROR("Allocator return invalid handle - DynamicArray: resize_internal");
                return false;
            }
            _data = maybe_handle.unwrap_copy();
            _capacity = new_capacity;
        } else if (new_capacity < _capacity) {
            if (new_capacity < _count) {
                move_ptr_backwards(_count - new_capacity);
            }
            _capacity = new_capacity;
        }

        return true;
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
    T* move_ptr_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = move_ptr_forward(1);
        construct_at(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    T* move_ptr_and_default_construct(u32 count) noexcept
    {
        T* place_ptr = move_ptr_forward(count);
        default_construct_at(place_ptr);
        return place_ptr;
    }

    void move_ptr_backwards(u32 move_count) noexcept
    {
        SF_ASSERT_MSG((move_count) <= _count, "Can't move more than all current elements");

        if constexpr (std::is_destructible_v<T>) {
            T* curr = last_ptr();

            for (u32 i{0}; i < move_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= move_count;
    }
}; // DynamicArray

} // sf
