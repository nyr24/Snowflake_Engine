#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_platform/platform.hpp"

namespace sf {

LinearAllocator::LinearAllocator() noexcept
    : _capacity{ platform_get_mem_page_size() * 10 }
    , _count{ 0 }
    , _buffer{ static_cast<u8*>(sf_mem_alloc(_capacity)) }
{}

LinearAllocator::LinearAllocator(usize capacity) noexcept
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

void* LinearAllocator::allocate(usize size, u16 alignment) noexcept
{
    usize padding = sf_calc_padding(_buffer + _count, alignment);

    if (_count + padding + size > _capacity) {
        u32 new_capacity = _capacity == 0 ? DEFAULT_INIT_CAPACITY : _capacity * 2;
        while (_count + padding + size > new_capacity) {
            new_capacity *= 2;
        }
        resize(new_capacity);
    }

    void* addr_to_return = _buffer + _count + padding;
    _count += padding + size;

    return addr_to_return;
}

usize LinearAllocator::allocate_handle(usize size, u16 alignment) noexcept
{
    return turn_ptr_into_handle(allocate(size, alignment), _buffer);
}

ReallocReturn LinearAllocator::reallocate(void* addr, usize new_size, u16 alignment) noexcept {
    if (addr == nullptr) {
        return {allocate(new_size, alignment), true};
    }

    if (!is_address_in_range(_buffer, _capacity, addr)) {
        return {nullptr, true};
    }

    return {allocate(new_size, alignment), true};
}

ReallocReturnHandle LinearAllocator::reallocate_handle(usize handle, usize new_size, u16 alignment) noexcept {
    if (handle == INVALID_ALLOC_HANDLE) {
        return {allocate_handle(new_size, alignment), true};
    }
    if (!is_handle_in_range(_buffer, _capacity, handle)) {
        return {INVALID_ALLOC_HANDLE, true};
    }
    return {allocate_handle(new_size, alignment), true};
}

void LinearAllocator::resize(usize new_capacity) noexcept {
    _buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));
    _capacity = new_capacity;
}

void* LinearAllocator::handle_to_ptr(usize handle) const noexcept {
#ifdef SF_DEBUG
    if (!is_handle_in_range(_buffer, _capacity, handle) || handle == INVALID_ALLOC_HANDLE) {
        return nullptr;
    }
#endif

    return _buffer + handle;
}

usize LinearAllocator::ptr_to_handle(void* ptr) const noexcept {
#ifdef SF_DEBUG
    if (!is_address_in_range(_buffer, _capacity, ptr) || ptr == nullptr) {
        return INVALID_ALLOC_HANDLE;
    }
#endif

    return turn_ptr_into_handle(ptr, _buffer);
}

void LinearAllocator::clear() noexcept
{
    _count = 0;
}

} // sf
