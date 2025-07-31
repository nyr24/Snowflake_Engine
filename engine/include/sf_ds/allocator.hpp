#pragma once

#include "sf_core/asserts_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include <type_traits>
#include <utility>

namespace sf {
u32 platform_get_mem_page_size();

// ArenaAllocator

// can't shrink or pop from this one, because of padding, and headers are not implemented on purpose
// for allocating global single-item stuff from the start of the program and deallocating all at once at the end
struct ArenaAllocator {
private:
    // in bytes
    u32 _capacity;
    // in bytes
    u32 _count;
    u8* _buffer;

public:
    ArenaAllocator() noexcept
        : _capacity{ platform_get_mem_page_size() }
        , _buffer{static_cast<u8*>(sf_mem_alloc(_capacity)) }
        , _count{ 0 }
    {}

    ArenaAllocator(u32 capacity) noexcept
        : _capacity{ capacity }
        , _buffer{static_cast<u8*>(sf_mem_alloc(capacity)) }
        , _count{ 0 }
    {}

    ArenaAllocator(ArenaAllocator&& rhs) noexcept
        : _buffer{ rhs._buffer }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
    {
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    ~ArenaAllocator() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity);
            _buffer = nullptr;
        }
    }

    template<typename T, typename... Args>
    T* allocate(Args&&... args) noexcept
    {
        constexpr u32 sizeo_of_T = sizeof(T);
        constexpr u32 align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        usize padding_bytes = reinterpret_cast<usize>(_buffer + _count) % align_of_T;
        if (_count + padding_bytes + sizeo_of_T > _capacity) {
            reallocate(_capacity * 2);
        }

        T* ptr_to_return = reinterpret_cast<T*>(_buffer + _count + padding_bytes);
        _count += padding_bytes + sizeo_of_T;

        construct(ptr_to_return, std::forward<Args>(args)...);

        return ptr_to_return;
    }

    void reallocate(u32 new_capacity) {
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity));
        sf_mem_copy(new_buffer, _buffer, _count);
        sf_mem_free(_buffer, _capacity);
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    u8* begin() noexcept { return _buffer; }
    u8* data() noexcept { return _buffer; }
    u8* end() noexcept { return _buffer + _count; }
    u32 count() const noexcept { return _count; }
    u32 capacity() const noexcept { return _capacity; }
    // const counterparts
    const u8* begin() const noexcept { return _buffer; }
    const u8* data() const noexcept { return _buffer; }
    const u8* end() const noexcept { return _buffer + _count; }

private:
    template<typename T, typename ...Args>
    void construct(T* ptr, Args&&... args) noexcept
    {
        sf_mem_place(ptr, args...);
    }
};

// DefaultArrayAllocator

// _capacity and _len are in elements, not bytes
template<typename T>
struct DefaultArrayAllocator {
private:
    u32 _capacity;
    u32 _count;
    T*  _buffer;

public:
    using ValueType = T;
    using Pointer   = T*;

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

    template<typename ...Args>
    void insert_at(u32 index, Args&&... args) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        T* item = _buffer + index;

        if constexpr (std::is_destructible_v<T>) {
            item->~T();
        }

        construct(item, std::forward<Args>(args)...);
    }

    void deallocate(u32 dealloc_count) noexcept
    {
        SF_ASSERT_MSG((dealloc_count) <= _count, "Can't deallocate more than all current elements");

        if constexpr (std::is_destructible_v<T>) {
            T* curr = end_before_one();

            for (u32 i{0}; i < dealloc_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= dealloc_count;
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

        sf_mem_move(item + 1, item, _count - index - 1);

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
        T* last = end_before_one();

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

    void pop() noexcept {
        deallocate(1);
    }

    void clear() noexcept {
        deallocate(_count);
    }

    void reallocate(u32 new_capacity) noexcept
    {
        if (new_capacity > _capacity) {
            T* new_buffer = sf_mem_alloc_typed<T, true>(new_capacity);
            sf_mem_copy(new_buffer, _buffer, _count * sizeof(T));
            sf_mem_free(_buffer, _capacity * sizeof(T), alignof(T));
            _capacity = new_capacity;
            _buffer = new_buffer;
        } else if (new_capacity < _capacity) {
            if (new_capacity < _count) {
                deallocate(_count - new_capacity);
            }
            _capacity = new_capacity;
        }
    }

    T* begin() noexcept { return _buffer; }
    T* end() noexcept { return _buffer + _count; }
    T* end_before_one() noexcept { return _buffer + _count - 1; }
    T* ptr_offset(u32 ind) noexcept { return _buffer + ind; }
    T& ptr_offset_val(u32 ind) noexcept { return *(ptr_offset(ind)); }
    u32 count() const noexcept { return _count; }
    u32 capacity() const noexcept { return _capacity; }
    u32 capacity_remain() const noexcept { return _capacity - _count; }
    // const counterparts
    const T* begin() const noexcept { return _buffer; }
    const T* end() const noexcept { return _buffer + _count; }
    const T* end_before_one() const noexcept { return _buffer + _count - 1; }
    const T* ptr_offset(u32 ind) const noexcept { return _buffer + ind; }
    const T& ptr_offset_val(u32 ind) const noexcept { return *(ptr_offset(ind)); }

    // constructors and assignments
    DefaultArrayAllocator() noexcept
        : _capacity{ 0 }
        , _count{ 0 }
        , _buffer{ nullptr }
    {}

    explicit DefaultArrayAllocator(u32 capacity) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {}

    explicit DefaultArrayAllocator(u32 capacity, u32 count) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {
        SF_ASSERT_MSG(capacity >= count, "Count shouldn't be bigger than capacity");
        allocate_and_default_construct(count);
    }

    DefaultArrayAllocator(DefaultArrayAllocator<T>&& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ rhs._buffer }
    {
        rhs._capacity = 0;
        rhs._count = 0;
        rhs._buffer = nullptr;
    }

    DefaultArrayAllocator<T>& operator=(DefaultArrayAllocator<T>&& rhs) noexcept
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

    DefaultArrayAllocator(const DefaultArrayAllocator<T>& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ sf_mem_alloc_typed<T, true>(rhs._capacity) }
    {
        sf_mem_copy(_buffer, rhs._buffer, rhs._count * sizeof(T));
    }

    DefaultArrayAllocator<T>& operator=(const DefaultArrayAllocator<T>& rhs) noexcept {
        if (_capacity < rhs._count) {
            sf_mem_free_typed<T, true>(_buffer);
            _buffer = sf_mem_alloc_typed<T, true>(rhs._capacity);
            _capacity = rhs._capacity;
        }
        _count = rhs._count;
        sf_mem_copy(_buffer, rhs._buffer, rhs._count);
    }

    friend bool operator==(const DefaultArrayAllocator<T>& first, const DefaultArrayAllocator<T>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    ~DefaultArrayAllocator() noexcept
    {
        if (_buffer) {
            sf_mem_free_typed<T, true>(_buffer);
            _buffer = nullptr;
        }
    }
}; // DefaultArrayAllocator


// FixedBufferAllocator

// _capacity and _len are in elements, not bytes
template<typename T, u32 Capacity>
struct FixedBufferAllocator {
private:
    u32 _count;
    T   _buffer[Capacity];

public:
    using ValueType = T;
    using Pointer   = T*;

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

    template<typename ...Args>
    void insert_at(u32 index, Args&&... args) noexcept {
        SF_ASSERT_MSG(index >= 0 && index < _count, "Out of bounds");

        T* item = _buffer + index;

        if constexpr (std::is_destructible_v<T>) {
            item->~T();
        }

        construct(item, std::forward<Args>(args)...);
    }

    void deallocate(u32 dealloc_count) noexcept
    {
        SF_ASSERT_MSG((dealloc_count) <= _count, "Can't deallocate more than all current elements");

        if constexpr (std::is_destructible_v<T>) {
            T* curr = end_before_one();

            for (u32 i{0}; i < dealloc_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= dealloc_count;
    }

    void reallocate(u32 new_capacity) noexcept
    {
        panic("Reallocation can't be made on stack based buffer");
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

        sf_mem_move(item + 1, item, _count - index - 1);

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
        T* last = end_before_one();

        if constexpr (std::is_destructible_v<T>) {
            item->~T();
        }

        *item = *last;
        --_count;
    }

    void pop() noexcept {
        deallocate(1);
    }

    void clear() noexcept {
        deallocate(_count);
    }

    T* begin() noexcept { return _buffer; }
    T* end() noexcept { return _buffer + _count; }
    T* end_before_one() noexcept { return _buffer + _count - 1; }
    T* ptr_offset(u32 ind) noexcept { return _buffer + ind; }
    T& ptr_offset_val(u32 ind) noexcept { return *(ptr_offset(ind)); }
    u32 count() const noexcept { return _count; }
    constexpr u32 capacity() const noexcept { return Capacity; }
    u32 capacity_remain() const noexcept { return Capacity - _count; }
    // const counterparts
    const T* begin() const noexcept { return _buffer; }
    const T* end() const noexcept { return _buffer + _count; }
    const T* end_before_one() const noexcept { return _buffer + _count - 1; }
    const T* ptr_offset(u32 ind) const noexcept { return _buffer + ind; }
    const T& ptr_offset_val(u32 ind) const noexcept { return *(ptr_offset(ind)); }

    // constructors and assignments
    FixedBufferAllocator() noexcept
        : _count{ 0 }
    {}

    // explicit FixedBufferAllocator(u32 count) noexcept
    //     : _count{ 0 }
    // {}

    explicit FixedBufferAllocator(u32 count) noexcept
        : _count{ count }
    {
        SF_ASSERT_MSG(Capacity >= count, "Count shouldn't be bigger than capacity");
        allocate_and_default_construct(count);
    }

    template<usize RhsCapacity>
    FixedBufferAllocator(const FixedBufferAllocator<T, RhsCapacity>& rhs) noexcept
        : _count{ rhs._count }
    {
        SF_ASSERT_MSG(_count == rhs._count, "Can't copy-assign buffers with different element lengths");
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);
    }

    template<usize RhsCapacity>
    FixedBufferAllocator<T, Capacity>& operator=(const FixedBufferAllocator<T, RhsCapacity>& rhs) noexcept {
        SF_ASSERT_MSG(_count == rhs._count, "Can't copy-assign buffers with different element lengths");
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);

        return *this;
    }

    // move constructor and assignments are complete copies of copy counterparts
    // because it does not make sence to move stack-based data
    // but we need them because of trait requirements
    template<usize RhsCapacity>
    FixedBufferAllocator(FixedBufferAllocator<T, RhsCapacity>&& rhs) noexcept
        : _count{ rhs._count }
    {
        SF_ASSERT_MSG(_count == rhs._count, "Can't move-assign buffers with different element lengths");
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);
    }

    template<usize RhsCapacity>
    FixedBufferAllocator<T, Capacity>& operator=(FixedBufferAllocator<T, RhsCapacity>&& rhs) noexcept
    {
        SF_ASSERT_MSG(_count == rhs._count, "Can't move-assign buffers with different element lengths");
        sf_mem_copy(_buffer, rhs._buffer, sizeof(T) * _count);

        return *this;
    }


    friend bool operator==(const FixedBufferAllocator<T, Capacity>& first, const FixedBufferAllocator<T, Capacity>& second) noexcept {
        return first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    ~FixedBufferAllocator() noexcept
    {}
}; // FixedBufferAllocator

} // sf
