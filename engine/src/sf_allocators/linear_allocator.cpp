#include "sf_allocators/linear_allocator.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_platform/platform.hpp"
#include <algorithm>

namespace sf {
    LinearAllocator::LinearAllocator() noexcept
        : _capacity{ platform_get_mem_page_size() * 10 }
        , _count{ 0 }
        , _buffer{ static_cast<u8*>(sf_mem_alloc(_capacity)) }
    {}

    LinearAllocator::LinearAllocator(u32 capacity) noexcept
        : _capacity{ capacity }
        , _count{ 0 }
        , _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    {}

    LinearAllocator::LinearAllocator(LinearAllocator&& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _buffer{ rhs._buffer }
    {
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }
        
        if (_buffer) {
            sf_mem_free(_buffer, _capacity);
            _buffer = nullptr;
        }

        _buffer = rhs._buffer;
        _capacity = rhs._capacity;
        _count = rhs._count;

        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;

        return *this;
    }

    LinearAllocator::~LinearAllocator() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity);
            _buffer = nullptr;
        }
    }

    void* LinearAllocator::allocate(u32 size, u16 alignment) noexcept
    {
        u32 padding = sf_calc_padding(_buffer + _count, alignment);
        if (_count + padding + size > _capacity) {
            resize(std::max(_capacity * 2, _count + padding + size));
        }

        void* addr_to_return = _buffer + _count + padding;
        _count += padding + size;

        return addr_to_return;
    }

    usize LinearAllocator::allocate_handle(u32 size, u16 alignment) noexcept
    {
        return turn_ptr_into_handle(allocate(size, alignment), _buffer);
    }

    void* LinearAllocator::reallocate(void* addr, u32 new_size, u16 alignment) noexcept {
        if (!is_address_in_range(_buffer, _capacity, addr)) {
            return nullptr;
        }

        return allocate(new_size, alignment);
    }

    Option<usize> LinearAllocator::reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept {
        if (handle == INVALID_ALLOC_HANDLE) {
            return allocate_handle(new_size, alignment);;
        }
        
        if (!is_handle_in_range(_buffer, _capacity, handle)) {
            return {None::VALUE};
        }

        return allocate_handle(new_size, alignment);
    }

    void LinearAllocator::resize(u32 new_capacity) noexcept {
        u8* new_buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    void* LinearAllocator::get_mem_with_handle(usize handle) const noexcept {
        if (!is_handle_in_range(_buffer, _capacity, handle) || handle == INVALID_ALLOC_HANDLE) {
            return nullptr;
        }

        return _buffer + handle;
    }

    void LinearAllocator::clear() noexcept
    {
        _count = 0;
    }

} // sf
