#include "sf_allocators/stack_allocator.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/memory_sf.hpp"
#include <algorithm>

namespace sf {

StackAllocator::StackAllocator() noexcept
    : _buffer{ static_cast<u8*>(sf_mem_alloc(DEFAULT_INIT_CAPACITY)) }
    , _capacity{ DEFAULT_INIT_CAPACITY }
    , _count{ 0 }
    , _prev_count{ 0 }
{}

StackAllocator::StackAllocator(u32 capacity) noexcept
    : _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    , _capacity{ capacity }
    , _count{ 0 }
    , _prev_count{ 0 }
{}

StackAllocator::StackAllocator(StackAllocator&& rhs) noexcept
    : _buffer{ rhs._buffer }
    , _capacity{ rhs._capacity }
    , _count{ rhs._count }
    , _prev_count{ 0 }
{
    rhs._buffer = nullptr;
    rhs._capacity = 0;
    rhs._count = 0;
    rhs._prev_count = 0;
}

StackAllocator& StackAllocator::operator=(StackAllocator&& rhs) noexcept
{
    if (this == &rhs) {
        return *this;
    }
    
    sf_mem_free(_buffer);
    _buffer = rhs._buffer;
    _capacity = rhs._capacity;
    _count = rhs._count;
    _prev_count = rhs._prev_count;

    rhs._buffer = nullptr;
    rhs._capacity = 0;
    rhs._count = 0;
    rhs._prev_count = 0;

    return *this;
}

StackAllocator::~StackAllocator() noexcept
{
    if (_buffer) {
        sf_mem_free(_buffer);
        _buffer = nullptr;
        _capacity = 0;
        _count = 0;
        _prev_count = 0;
    }
}

void* StackAllocator::allocate(u32 size, u16 alignment) noexcept
{
    u32 padding = calc_padding_with_header(_buffer + _count, alignment, sizeof(StackAllocatorHeader));

    if (_count + padding + size > _capacity) {
        u32 new_capacity = std::max(_capacity * 2, _count + padding + size);
        resize(new_capacity);
    }

    StackAllocatorHeader* header = reinterpret_cast<StackAllocatorHeader*>(_buffer + _count + (padding - sizeof(StackAllocatorHeader)));
    header->diff = _count - _prev_count;
    header->padding = padding;

    void* ptr_to_ret = _buffer + _count + padding;
    _prev_count = _count;
    _count += padding + size;
    return ptr_to_ret;
}

usize StackAllocator::allocate_handle(u32 size, u16 alignment) noexcept
{
    void* ptr = allocate(size, alignment);
    return turn_ptr_into_handle(ptr, _buffer);  
}

void* StackAllocator::reallocate(void* addr, u32 new_size, u16 alignment) noexcept
{
    if (addr == nullptr) {
        return allocate(new_size, alignment);
    } else if (new_size == 0) {
        free(addr);
        return nullptr;
    } else {
        if (!is_address_in_range(_buffer, _capacity, addr)) {
            return nullptr;
        }
        // check if it is the last allocation -> just grow/shrink this chunk of memory
        StackAllocatorHeader* header = static_cast<StackAllocatorHeader*>(ptr_step_bytes_backward(addr, sizeof(StackAllocatorHeader)));
        u32 prev_offset = turn_ptr_into_handle(static_cast<StackAllocatorHeader*>(ptr_step_bytes_backward(addr, header->padding)), _buffer);

        if (_prev_count == prev_offset) {
            u32 prev_size = _count - _prev_count;
            // grow
            if (new_size > prev_size) {
                u32 size_diff = new_size - prev_size;
                u32 remain_space = _capacity - _count;
                if (remain_space < size_diff) {
                    resize(_capacity * 2);
                }
                _count += size_diff;
                return addr;
            }
            // shrink
            else {
                u32 size_diff = prev_size - new_size;
                _count -= size_diff;
                return addr;
            }
        } else {
            // alloc new memory block
            free(addr);
            return allocate(new_size, alignment);
        }
    }
}

usize StackAllocator::reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept
{
    if (handle == INVALID_ALLOC_HANDLE) {
        void* addr = allocate(new_size, alignment);
        if (!addr) {
            return INVALID_ALLOC_HANDLE;
        }
        return turn_ptr_into_handle(addr, _buffer);
    }
    
    void* addr = reallocate(turn_handle_into_ptr(handle, _buffer), new_size, alignment);
    if (!addr) {
        return INVALID_ALLOC_HANDLE;
    }
    
    return turn_ptr_into_handle(addr, _buffer);
}

void StackAllocator::clear() noexcept
{
    _count = 0;
}

void StackAllocator::free(void* addr) noexcept {
    if (!is_address_in_range(_buffer, _capacity, addr)) {
        return;
    }

    StackAllocatorHeader* header = static_cast<StackAllocatorHeader*>(ptr_step_bytes_backward(addr, sizeof(StackAllocatorHeader)));
    u32 prev_offset = turn_ptr_into_handle(static_cast<StackAllocatorHeader*>(ptr_step_bytes_backward(addr, header->padding)), _buffer);
    if (_prev_count != prev_offset) {
        return;
    }

    _count = prev_offset;
    _prev_count -= header->diff;
}

void StackAllocator::resize(u32 new_capacity) noexcept {
    u8* new_buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));
    _capacity = new_capacity;
    _buffer = new_buffer;
}

void* StackAllocator::get_mem_with_handle(usize handle) const noexcept {
    if (!is_handle_in_range(_buffer, _capacity, handle)) {
        return nullptr;
    }

    return _buffer + handle;
}

void StackAllocator::free_handle(usize handle) noexcept {
    if (handle == INVALID_ALLOC_HANDLE) {
        return;
    }

    if (!is_handle_in_range(_buffer, _capacity, handle)) {
        return;
    }

    free(turn_handle_into_ptr(handle, _buffer));
}

} // sf
