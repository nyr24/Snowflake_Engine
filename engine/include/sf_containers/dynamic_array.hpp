#pragma once

#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/iterator.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/constants.hpp"
#include <initializer_list>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sf {

// _capacity and _len are in elements, not bytes
template<typename T, AllocatorTrait Allocator = GeneralPurposeAllocator, bool USE_HANDLE = true, f32 GROW_FACTOR = 2.0f, u32 DEFAULT_CAPACITY = 8>
struct DynamicArrayBacked {
protected:
    union Data {
        T*     ptr;
        u32    handle;
    };
    
    Allocator*   _allocator;
    Data         _data;
    u32          _capacity;
    u32          _count;

public:
    using ValueType     = T;
    using PointerType   = T*;

public:
    DynamicArrayBacked() noexcept
        : _allocator{nullptr}
        , _capacity{0}
        , _count{0}
    {
        if constexpr (USE_HANDLE) {
            _data.handle = INVALID_ALLOC_HANDLE;
        } else {
            _data.ptr = nullptr;
        }
    }

    explicit DynamicArrayBacked(Allocator* allocator) noexcept
        : _allocator{allocator}
        , _capacity{0}
        , _count{0}
    {
        if constexpr (USE_HANDLE) {
            _data.handle = INVALID_ALLOC_HANDLE;
        } else {
            _data.ptr = nullptr;
        }
    }

    explicit DynamicArrayBacked(u32 capacity_input, Allocator* allocator) noexcept
        : _allocator{allocator}
        , _capacity{capacity_input}
        , _count{0}
    {
        if (_allocator) {
            if constexpr (USE_HANDLE) {
                _data.handle = allocator->allocate_handle(capacity_input * sizeof(T), alignof(T));
            } else {
                _data.ptr = static_cast<T*>(allocator->allocate(capacity_input * sizeof(T), alignof(T)));
            }
        } else {
            LOG_ERROR("DynamicArrayBacked: provided allocator is nullptr");
        }
    }

    explicit DynamicArrayBacked(u32 capacity_input, u32 count, Allocator* allocator) noexcept
        : _allocator{allocator}
        , _capacity{capacity_input}
        , _count{0}
    {
        SF_ASSERT_MSG(capacity_input >= count, "Count shouldn't be bigger than capacity");

        if (_allocator) {
            if constexpr (USE_HANDLE) {
                _data.handle = allocator->allocate_handle(capacity_input * sizeof(T), alignof(T));
            } else {
                _data.ptr = static_cast<T*>(allocator->allocate(capacity_input * sizeof(T), alignof(T)));
            }
            move_forward(count);
        } else {
            LOG_ERROR("DynamicArrayBacked: provided allocator is nullptr");
        }
    }

    DynamicArrayBacked(std::tuple<Allocator*, std::initializer_list<T>>&& config) noexcept
        : _allocator{std::get<0>(config)}
        , _capacity{static_cast<u32>(std::get<1>(config).size())}
        , _count{0}
    {
        if (_allocator) {
            auto& init_list{ std::get<1>(config) };

            if constexpr (USE_HANDLE) {
                _data.handle = _allocator->allocate_handle(init_list.size() * sizeof(T), alignof(T));
            } else {
                _data.ptr = static_cast<T*>(_allocator->allocate(init_list.size() * sizeof(T), alignof(T)));
            }

            T* data{ access_data() };
            move_forward(init_list.size());

            u32 i{0};
            for (auto it = init_list.begin(); it != init_list.end(); ++it, ++i) {
                if constexpr (std::is_trivial_v<T>) {
                    data[i] = *it;
                } else {
                    construct_at(data + i, *it);
                }
            }
        } else {
            LOG_ERROR("DynamicArrayBacked: provided allocator is nullptr");
        }
    }


    DynamicArrayBacked(std::tuple<Allocator*, u32, std::initializer_list<T>>&& config) noexcept
        : _allocator{std::get<0>(config)}
        , _capacity{static_cast<u32>(std::get<1>(config))}
        , _count{0}
    {
        auto& init_list{ std::get<2>(config) };
        SF_ASSERT_MSG(init_list.size() <= _capacity, "Initializer list size don't fit for specified capacity");

        if (_allocator) {
            auto& init_list{ std::get<1>(config) };

            if constexpr (USE_HANDLE) {
                _data.handle = _allocator->allocate_handle(init_list.size() * sizeof(T), alignof(T));
            } else {
                _data.ptr = static_cast<T*>(_allocator->allocate(init_list.size() * sizeof(T), alignof(T)));
            }

            T* data{ access_data() };
            move_forward(init_list.size());

            u32 i{0};
            for (auto it = init_list.begin(); it != init_list.end(); ++it, ++i) {
                if constexpr (std::is_trivial_v<T>) {
                    data[i] = *it;
                } else {
                    construct_at(data + i, *it);
                }
            }
        } else {
            LOG_ERROR("DynamicArrayBacked: provided allocator is nullptr");
        }
    }

    DynamicArrayBacked(DynamicArrayBacked<T, Allocator>&& rhs) noexcept
        : _allocator{rhs._allocator}
        , _data{rhs._data}
        , _capacity{rhs._capacity}
        , _count{rhs._count}
    {   
        rhs._capacity = 0;
        if constexpr (USE_HANDLE) {
            rhs._data.handle = INVALID_ALLOC_HANDLE;
        } else {
            rhs._data.ptr = nullptr;
        }
        rhs._count = 0;
    }

    DynamicArrayBacked<T, Allocator>& operator=(DynamicArrayBacked<T, Allocator>&& rhs) noexcept
    {
        if (this == &rhs) return *this;

        T* data = access_data();
        if (_allocator && data) {
            _allocator->free(data);
        }
        _allocator = rhs._allocator;
        _capacity = rhs._capacity;
        _count = rhs._count;
        _data= rhs._data;
        rhs._capacity = 0;
        if constexpr (USE_HANDLE) {
            rhs._data.handle = INVALID_ALLOC_HANDLE;
        } else {
            rhs._data.ptr = nullptr;
        }
        rhs._count = 0;

        return *this;
    }

    DynamicArrayBacked(const DynamicArrayBacked<T, Allocator>& rhs) noexcept
        : _allocator{rhs._allocator}
        , _data{rhs._data}
        , _capacity{rhs._capacity}
        , _count{rhs._count}
    {
        if (rhs._allocator && rhs._count > 0) {
            if constexpr (USE_HANDLE) {
                _data.handle = _allocator->allocate_handle(rhs._capacity * sizeof(T), alignof(T));
            } else {
                _data.ptr = static_cast<T*>(_allocator->allocate(rhs._capacity * sizeof(T), alignof(T)));
            }
            sf_mem_copy((void*)access_data(), (void*)rhs.access_data(), rhs._count * sizeof(T));
        }
    }

    DynamicArrayBacked<T, Allocator>& operator=(const DynamicArrayBacked<T, Allocator>& rhs) noexcept {
        if (this == &rhs) return *this;

        T* data = access_data();
        if (_allocator && data) {
            _allocator->free(data);
        }

        _allocator = rhs._allocator;
        if (_capacity < rhs._capacity) {
            grow(rhs._capacity);
        }
        _count = rhs._count;
        if (rhs._count > 0) {
            sf_mem_copy((void*)data, (void*)rhs.access_data(), rhs._count * sizeof(T));
        }

        return *this;
    }

    ~DynamicArrayBacked() noexcept
    {
        free();
    }

    void free() noexcept {
        if constexpr (USE_HANDLE) {
            if (_data.handle != INVALID_ALLOC_HANDLE) {
                _allocator->free_handle(_data.handle);
                _data.handle = INVALID_ALLOC_HANDLE;
            }
        } else {
            if (_data.ptr) {
                _allocator->free(_data.ptr);
                _data.ptr = nullptr;
            }
        }
    }

    void set_allocator(Allocator* allocator) noexcept
    {
        if (allocator) {
            _allocator = allocator;
        } else {
            LOG_ERROR("DynamicArrayBacked : set_allocator(): provided allocator is nullptr");
        }
    }
    
    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");       
        move_forward_and_construct(std::forward<Args>(args)...);
    }

    void append(const T& item) noexcept {
        if constexpr (std::is_trivial_v<T>) {
            move_forward(1);
            T* data = access_data();
            data[_count - 1] = item;
        } else {
            move_forward_and_construct(item);
        }
    }

    void append(T&& item) noexcept {
        if constexpr (std::is_trivial_v<T>) {
            move_forward(1);
            T* data = access_data();
            data[_count - 1] = item;
        } else {
            move_forward_and_construct(std::move(item));
        }
    }

    void append_slice(std::span<T> sp) noexcept {
        move_forward(sp.size());
        sf_mem_copy(access_data() + (_count - sp.size()), (void*)sp.data(), sizeof(T) * sp.size());
    }

    void remove_at(u32 index) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        if (index == _count - 1) {
            move_ptr_backwards(1);
            return;
        }

        T* data = access_data();
        T* item = data + index;

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

        T* data = access_data();
        T* item = data + index;
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

    constexpr void pop_range(u32 count) noexcept {
        SF_ASSERT_MSG(count <= _count, "Can't pop more than have");
        move_ptr_backwards(count);
    }

    constexpr void clear() noexcept {
        move_ptr_backwards(_count);
    }

    constexpr void fill(ConstLRefOrValType<T> val) noexcept {
        T* data = access_data();
        for (u32 i{0}; i < _capacity; ++i) {
            data[i] = val;
        }
        _count = _capacity;
    }

    void reserve(u32 new_capacity) noexcept {
        if (new_capacity > _capacity) {
            grow(new_capacity, true);
        }
    }

    void reserve_exponent(u32 new_capacity) noexcept {
        if (new_capacity > _capacity) {
            grow(new_capacity, false);
        }
    }

    void resize(u32 new_count) noexcept {
        if (!_allocator) {
            LOG_ERROR("DynamicArrayBacked : resize(): _allocator should be valid pointer");
            return;
        }

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

    void resize_exponent(u32 new_count) noexcept
    {
        if (!_allocator) {
            LOG_ERROR("DynamicArrayBacked : reserve_and_resize_exponent(): _allocator should be valid pointer");
            return;
        }

        if (new_count > _capacity) {
            grow(new_count, false);
            resize_to_capacity();
        }
    }

    void resize_to_capacity() noexcept {
        if (!_allocator) {
            LOG_ERROR("DynamicArrayBacked : resize(): _allocator should be valid pointer");
            return;
        }
        if (_count < _capacity) {
            const u32 diff = _capacity - _count;
            if constexpr (std::is_trivial_v<T>) {
                move_forward(diff);
            } else {
                move_forward_and_default_construct(diff);
            }
        }
    }

    void reserve_and_resize(u32 new_capacity, u32 new_count) noexcept
    {
        SF_ASSERT_MSG(new_capacity >= new_count, "Invalid resize count");

        if (!_allocator) {
            LOG_ERROR("DynamicArrayBacked : reserve_and_resize(): _allocator should be valid pointer");
            return;
        }

        if (new_capacity > _capacity) {
            grow(new_capacity, true);
        }

        if (new_count > _count) {
            if constexpr (std::is_trivial_v<T>) {
                move_forward(new_count - _count);
            } else {
                move_forward_and_default_construct(new_count - _count);
            }
        }
    }

    std::span<T> to_span(u32 start = 0, u32 len = 0) noexcept {
        return std::span{ access_data() + start, len == 0 ? _count : len };
    }

    std::span<const T> to_span(u32 start = 0, u32 len = 0) const noexcept {
        return std::span{ access_data() + start, len == 0 ? _count : len };
    }

    constexpr bool is_empty() const { return _count == 0; }
    constexpr bool is_full() const { return _count == _capacity; }
    constexpr T* data() noexcept { return access_data(); }
    constexpr T* data_offset(u32 i) noexcept { return access_data() + i; }
    constexpr T& first() noexcept { return *(access_data()); }
    constexpr T& last() noexcept { return *(access_data() + _count - 1); }
    constexpr T& last_past_end() noexcept { return *(access_data() + _count); }
    constexpr T* first_ptr() noexcept { return access_data(); }
    constexpr T* last_ptr() noexcept { return access_data() + _count - 1; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 size_in_bytes() const noexcept { return sizeof(T) * _count; }
    constexpr u32 capacity() const noexcept { return _capacity; }
    constexpr u32 capacity_remain() const noexcept { return _capacity - _count; }
    // const counterparts
    const T* data() const noexcept { return access_data(); }
    const T* data_offset(u32 i) const noexcept { return access_data() + i; }
    const T& first() const noexcept { return *(access_data()); }
    const T& last() const noexcept { return *(access_data() + _count - 1); }
    const T& last_past_end() const noexcept { return *(access_data() + _count); }
    const T* first_ptr() const noexcept { return access_data(); }
    const T* last_ptr() const noexcept { return access_data() + _count - 1; }

    constexpr PtrRandomAccessIterator<T> begin() const noexcept {
        return PtrRandomAccessIterator<T>(access_data());
    }

    constexpr PtrRandomAccessIterator<T> end() const noexcept {
        return PtrRandomAccessIterator<T>(access_data() + _count);
    }

    T& operator[](u32 ind) noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return access_data()[ind];
    }

    const T& operator[](u32 ind) const noexcept {
        SF_ASSERT_MSG(ind >= 0 && ind < _count, "Out of bounds");
        return access_data()[ind];
    }

    friend bool operator==(
        const DynamicArrayBacked<T, Allocator, USE_HANDLE, GROW_FACTOR, DEFAULT_CAPACITY>& first,
        const DynamicArrayBacked<T, Allocator, USE_HANDLE, GROW_FACTOR, DEFAULT_CAPACITY>& second) noexcept
    {
        return first._count == second._count && sf_mem_cmp(first.access_data(), second.access_data(), first._count * sizeof(T));
    }

    static u64 hash(const DynamicArrayBacked<T, Allocator>& key) noexcept {
        constexpr u64 PRIME = 1099511628211u;
        constexpr u64 OFFSET_BASIS = 14695981039346656037u;

        u64 hash = OFFSET_BASIS;

        for (u32 i{0}; i < key.count() * sizeof(T); ++i) {
            hash ^= reinterpret_cast<u8*>(key.data())[i];
            hash *= PRIME;
        }

        return hash;
    }

    void shrink(u32 new_capacity) noexcept {
        SF_ASSERT_MSG(_allocator, "Allocator should be set");
        if (new_capacity < _count) {
            move_ptr_backwards(_count - new_capacity);
        }
        _capacity = new_capacity;
    }

    bool has(ConstLRefOrValType<T> item) noexcept {
        T* data = access_data();
        for (u32 i{0}; i < _count; ++i) {
            if (data[i] == item) {
                return true;
            }
        }

        return false;
    }

    Option<u32> index_of(ConstLRefOrValType<T> item) noexcept {
        T* data = access_data();
        for (u32 i{0}; i < _count; ++i) {
            if (data[i] == item) {
                return i;
            }
        }

        return {None::VALUE};
    }

protected:
    T* access_data() const {
        if constexpr (USE_HANDLE) {
            return static_cast<T*>(_allocator->handle_to_ptr(_data.handle));
        } else {
            return _data.ptr;
        } 
    }

    void grow(u32 new_capacity, bool exact = false) noexcept {
        SF_ASSERT_MSG(_allocator, "Allocator should be set");
        if (_capacity >= new_capacity) {
            return;
        }

        u32 old_capacity = _capacity;
        if (!exact) {
            if (_capacity == 0) {
                _capacity = DEFAULT_CAPACITY;
            }
            while (_capacity < new_capacity) {
                _capacity *= GROW_FACTOR;
            }
        } else {
            _capacity = new_capacity;
        }

        if constexpr (USE_HANDLE) {
            ReallocReturnHandle realloc_res = _allocator->reallocate_handle(_data.handle, _capacity * sizeof(T), alignof(T));
            if (realloc_res.should_mem_copy && old_capacity > 0) {
                sf_mem_copy((void*)(_allocator->handle_to_ptr(realloc_res.handle)), (void*)(_allocator->handle_to_ptr(_data.handle)), old_capacity);
            }
            _data.handle = realloc_res.handle;
        } else {
            ReallocReturn realloc_res = _allocator->reallocate(_data.ptr, _capacity * sizeof(T), alignof(T));
            if (realloc_res.should_mem_copy && old_capacity > 0) {
                sf_mem_copy((void*)realloc_res.ptr, (void*)_data.ptr, old_capacity);
            }
            _data.ptr = static_cast<T*>(realloc_res.ptr);
        }         
    }
    
    T* move_ptr_forward(u32 alloc_count) noexcept
    {
        u32 free_capacity = _capacity - _count;
        if (free_capacity < alloc_count) {
            grow(_capacity * GROW_FACTOR);
        }
        T* return_memory = access_data() + _count;
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
    void move_forward_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = move_ptr_forward(1);
        construct_at(place_ptr, std::forward<Args>(args)...);
    }

    T* move_forward_and_default_construct(u32 count) noexcept
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
}; // DynamicArrayBacked

template<AllocatorTrait Allocator, bool USE_HANDLE = true, f32 GROW_FACTOR = 2.0f, u32 DEFAULT_CAPACITY = 8>
struct StringBacked : public DynamicArrayBacked<char, Allocator, USE_HANDLE, GROW_FACTOR, DEFAULT_CAPACITY>
{
    using DynamicArrayBacked<char, Allocator>::DynamicArrayBacked;
    
    std::string_view to_string_view(u32 start = 0, u32 len = 0) noexcept {
        return std::string_view{ this->access_data() + start, len == 0 ? this->_count : len };
    }

    std::string_view to_string_view(u32 start = 0, u32 len = 0) const noexcept {
        return std::string_view{ static_cast<const char*>(this->access_data() + start), len == 0 ? this->_count : len };
    }

    void append_sv(std::string_view sv) noexcept {
        this->move_forward(sv.size());
        sf_mem_copy(this->access_data() + (this->_count - sv.size()), (void*)sv.data(), sizeof(char) * sv.size());
    }

    void trim_end(char trimmer = ' ') noexcept {
        while (this->_count > 0 && (this->last() == trimmer)) {
            this->_count--;
        }
    }

    void concat(StringBacked<Allocator>& rhs) noexcept {
        u32 old_count = this->_scount;
        this->reserve_and_resize(this->_count + rhs->_count);
        sf_mem_copy((void*)(this->data_offset(old_count)), (void*)rhs.data(), rhs.size_in_bytes());
    }

    void ensure_null_terminated() noexcept {
        if (!is_null_terminated()) {
            this->append('\0');
        }
    }

    bool is_null_terminated() noexcept {
        return this->last() == '\0';
    }
};

// Backed

} // sf
