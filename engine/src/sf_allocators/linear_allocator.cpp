#include "sf_allocators/linear_allocator.hpp"

namespace sf {
    LinearAllocator::LinearAllocator() noexcept
        : _capacity{ platform_get_mem_page_size() * 10 }
        , _buffer{ static_cast<u8*>(sf_mem_alloc(_capacity)) }
        , _count{ 0 }
    {}

    LinearAllocator::LinearAllocator(u32 capacity) noexcept
        : _capacity{ capacity }
        , _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
        , _count{ 0 }
    {}

    LinearAllocator::LinearAllocator(LinearAllocator&& rhs) noexcept
        : _buffer{ rhs._buffer }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
    {
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    LinearAllocator::~LinearAllocator() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity);
            _buffer = nullptr;
        }
    }

    u32 LinearAllocator::allocate(u32 size, u16 alignment) noexcept
    {
        u32 padding = sf_calc_padding(_buffer + _count,  alignment);
        if (_count + padding + size > _capacity) {
            reallocate(_capacity * 2);
        }

        u32 handle_to_return = _count + padding;
        _count += padding + size;

        return handle_to_return;
    }

    void LinearAllocator::reallocate(u32 new_capacity) noexcept {
        u8* new_buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));
        if (_buffer != new_buffer) {
            sf_mem_free(_buffer, _capacity);
        }
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    void* LinearAllocator::get_with_handle(u32 handle) const noexcept {
        if (!is_valid_handle(handle)) {
            return nullptr;
        }

        return _buffer + handle;
    }

    bool LinearAllocator::is_valid_handle(u32 handle) const noexcept {
        return handle < _count;
    }

} // sf
