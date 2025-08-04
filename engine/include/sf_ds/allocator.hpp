#pragma once

#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
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

} // sf
