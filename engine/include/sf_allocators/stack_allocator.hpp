#pragma once

#include "sf_core/defines.hpp"
#include "sf_containers/optional.hpp"

namespace sf {

struct StackAllocatorHeader {
    // offset - prev_offset at the moment of allocation
    u16 diff;
    u16 padding;
};

struct StackAllocator {
static constexpr u32 DEFAULT_INIT_CAPACITY{ 1024};
private:
    u8* _buffer;
    u32 _capacity;
    u32 _count;
    u32 _prev_count;
public:
    StackAllocator() noexcept;
    StackAllocator(u32 capacity) noexcept;
    StackAllocator(StackAllocator&& rhs) noexcept;
    StackAllocator& operator=(StackAllocator&& rhs) noexcept;
    ~StackAllocator() noexcept;
    
    void* allocate(u32 size, u16 alignment) noexcept;
    usize allocate_handle(u32 size, u16 alignment) noexcept;
    void* reallocate(void* addr, u32 new_size, u16 alignment) noexcept;
    Option<usize> reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept;
    void clear() noexcept;
    void free(void* addr) noexcept;
    void free_handle(usize handle) noexcept;
    void* get_mem_with_handle(usize handle) const noexcept;

    constexpr u8* begin() noexcept { return _buffer; }
    constexpr u8* data() noexcept { return _buffer; }
    constexpr u8* end() noexcept { return _buffer + _count; }
    constexpr u32 count() const noexcept { return _count; }
    constexpr u32 capacity() const noexcept { return _capacity; }
    // const counterparts
    const u8* begin() const noexcept { return _buffer; }
    const u8* data() const noexcept { return _buffer; }
    const u8* end() const noexcept { return _buffer + _count; }

private:
    void resize(u32 new_capacity) noexcept;
    void free_last_alloc(void* addr) noexcept;
    void free_last_alloc_handle(u32 handle) noexcept;
};

} // sf
