#pragma once

#include "sf_core/memory_sf.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/utility.hpp"
#include <utility>

namespace sf {

// ArenaAllocator

// can't shrink or pop from this one, because of padding, and headers are not implemented on purpose
// for allocating global single-item stuff from the start of the program and deallocating all at once at the end
struct ArenaAllocator {
private:
    // in bytes
    usize   _capacity;
    // in bytes
    usize   _count;
    u8*     _buffer;

public:
    ArenaAllocator() noexcept
        : _capacity{ platform_get_mem_page_size() }
        , _buffer{static_cast<u8*>(sf_mem_alloc(_capacity)) }
        , _count{ 0 }
    {}

    ArenaAllocator(usize capacity) noexcept
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
        constexpr usize sizeo_of_T = sizeof(T);
        constexpr usize align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        u32 padding_bytes = reinterpret_cast<usize>(_buffer + _count) % align_of_T;
        if (_count + padding_bytes + sizeo_of_T > _capacity) {
            this->reallocate(_capacity * 2);
        }

        T* ptr_to_return = reinterpret_cast<T*>(_buffer + _count + padding_bytes);
        _count += padding_bytes + sizeo_of_T;

        this->construct(ptr_to_return, std::forward<Args>(args)...);

        return ptr_to_return;
    }

    void reallocate(usize new_capacity) {
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity));
        sf_mem_copy(new_buffer, _buffer, _count);
        sf_mem_free(_buffer, _capacity);
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    u8* begin() noexcept { return _buffer; }
    u8* data() noexcept { return _buffer; }
    u8* end() noexcept { return _buffer + _count; }
    usize count() const noexcept { return _count; }
    usize capacity() const noexcept { return _capacity; }
    // const counterparts
    const u8* begin() const noexcept { return _buffer; }
    const u8* data() const noexcept { return _buffer; }
    const u8* end() const noexcept { return _buffer + _count; }

private:
    template<typename T, typename ...Args>
    void construct(T* ptr, Args&&... args) noexcept
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }
};


// DefaultArrayAllocator

// _capacity and _len are in elements, not bytes
template<typename T>
struct DefaultArrayAllocator {
private:
    usize   _capacity;
    usize   _count;
    T*      _buffer;

public:
    using ValueType = T;
    using Pointer   = T*;

    T* allocate(u64 alloc_count) noexcept
    {
        usize free_capacity = _capacity - _count;

        if (free_capacity < alloc_count) {
            this->reallocate(_capacity * 2);
        }

        T* return_memory = _buffer + _count;
        _count += alloc_count;

        return return_memory;
    }

    template<typename ...Args>
    void construct(T* ptr, Args&&... args) noexcept
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    T* allocate_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = allocate(1);
        construct(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    void deallocate(u64 dealloc_count) noexcept
    {
        SF_ASSERT_MSG((dealloc_count) <= _count, "Can't deallocate more than all current elements");

        if constexpr (std::is_destructible<T>::value) {
            T* curr = this->end_before_one();

            for (usize i{0}; i < dealloc_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= dealloc_count;
    }

    template<typename ...Args>
    void destroy(T* ptr, Args&&... args) noexcept
    {
        ::operator delete(ptr, std::nothrow);
    }

    void reallocate(usize new_capacity) noexcept
    {
        T* new_buffer = sf_mem_alloc_typed<T, true>(new_capacity);
        sf_mem_copy(new_buffer, _buffer, _count * sizeof(T));
        sf_mem_free(_buffer, _capacity * sizeof(T), alignof(T));
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    void pop() noexcept {
        this->deallocate(1);
    }

    void clear() noexcept {
        this->deallocate(this->count());
    }

    T* begin() noexcept { return _buffer; }
    T* end() noexcept { return _buffer + _count; }
    T* end_before_one() noexcept { return _buffer + _count - 1; }
    T* ptr_offset(usize ind) noexcept { return _buffer + ind; }
    T& ptr_offset_val(usize ind) noexcept { return *(ptr_offset(ind)); }
    usize count() const noexcept { return _count; }
    usize capacity() const noexcept { return _capacity; }
    // const counterparts
    const T* begin() const noexcept { return _buffer; }
    const T* end() const noexcept { return _buffer + _count; }
    const T* end_before_one() const noexcept { return _buffer + _count - 1; }
    const T* ptr_offset(usize ind) const noexcept { return _buffer + ind; }
    const T& ptr_offset_val(usize ind) const noexcept { return *(ptr_offset(ind)); }

    // constructors and assignments

    DefaultArrayAllocator(u64 count) noexcept
        : _capacity{ count }
        , _count{ 0 }
        , _buffer{ sf_mem_alloc_typed<T, true>(_capacity) }
    {}

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
template<typename T, usize Capacity>
struct FixedBufferAllocator {
private:
    usize   _count;
    T       _buffer[Capacity];

public:
    using ValueType = T;
    using Pointer   = T*;

    T* allocate(u64 alloc_count) noexcept
    {
        usize free_capacity = Capacity - _count;

        if (free_capacity < alloc_count) {
            panic("Not enough memory");
        }

        T* return_memory = _buffer + _count;
        _count += alloc_count;

        return return_memory;
    }

    template<typename ...Args>
    void construct(T* ptr, Args&&... args) noexcept
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    T* allocate_and_construct(Args&&... args) noexcept
    {
        T* place_ptr = allocate(1);
        construct(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    void deallocate(u64 dealloc_count) noexcept
    {
        SF_ASSERT_MSG((dealloc_count) <= _count, "Can't deallocate more than all current elements");

        if constexpr (std::is_destructible<T>::value) {
            T* curr = this->end_before_one();

            for (usize i{0}; i < dealloc_count; ++i) {
                (curr - i)->~T();
            }
        }

        _count -= dealloc_count;
    }

    template<typename ...Args>
    void destroy(T* ptr, Args&&... args) noexcept
    {
        ::operator delete(ptr, std::nothrow);
    }

    void reallocate(usize new_capacity) noexcept
    {
        panic("Reallocation can't be made on stack based buffer");
    }

    void pop() noexcept {
        this->deallocate(1);
    }

    void clear() noexcept {
        this->deallocate(_count);
    }

    T* begin() noexcept { return _buffer; }
    T* end() noexcept { return _buffer + _count; }
    T* end_before_one() noexcept { return _buffer + _count - 1; }
    T* ptr_offset(usize ind) noexcept { return _buffer + ind; }
    T& ptr_offset_val(usize ind) noexcept { return *(ptr_offset(ind)); }
    usize count() const noexcept { return _count; }
    constexpr usize capacity() const noexcept { return Capacity; }
    // const counterparts
    const T* begin() const noexcept { return _buffer; }
    const T* end() const noexcept { return _buffer + _count; }
    const T* end_before_one() const noexcept { return _buffer + _count - 1; }
    const T* ptr_offset(usize ind) const noexcept { return _buffer + ind; }
    const T& ptr_offset_val(usize ind) const noexcept { return *(ptr_offset(ind)); }

    // constructors and assignments
    FixedBufferAllocator() noexcept
        : _count{ 0 }
    {}

    FixedBufferAllocator(u64 count) noexcept
        : _count{ 0 }
    {}

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


    template<usize LhsCapacity, usize RhsCapacity>
    friend bool operator==(const FixedBufferAllocator<T, LhsCapacity>& first, const FixedBufferAllocator<T, RhsCapacity>& second) noexcept {
        return LhsCapacity == RhsCapacity && first._count == second._count && sf_mem_cmp(first._buffer, second._buffer, first._count * sizeof(T));
    }

    ~FixedBufferAllocator() noexcept
    {}
}; // FixedBufferAllocator

} // sf
