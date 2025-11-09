#pragma once

#include "sf_containers/traits.hpp"
#include "sf_core/defines.hpp"

namespace sf {

struct LinearAllocator {
private:
    u32 _capacity;
    u32 _count;
    u8* _buffer;

public:
    LinearAllocator() noexcept;
    LinearAllocator(u32 capacity) noexcept;
    LinearAllocator(LinearAllocator&& rhs) noexcept;
    LinearAllocator& operator=(LinearAllocator&& rhs) noexcept;
    ~LinearAllocator() noexcept;

    void* allocate(u32 size, u16 alignment) noexcept;
    usize allocate_handle(u32 size, u16 alignment) noexcept;
    ReallocReturn reallocate(void* addr, u32 new_size, u16 alignment) noexcept;
    ReallocReturnHandle reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept;
    void* handle_to_ptr(usize handle) const noexcept;
    usize ptr_to_handle(void* ptr) const noexcept;
    void clear() noexcept;
    void free(void* addr) noexcept {}
    void free_handle(usize handle) noexcept {}

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
};

} // sf
