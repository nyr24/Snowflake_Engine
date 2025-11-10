#pragma once

#include "sf_containers/traits.hpp"
#include "sf_core/defines.hpp"

namespace sf {

struct LinearAllocator {
public:
static constexpr usize DEFAULT_INIT_CAPACITY{1024};
private:
    usize _capacity;
    usize _count;
    u8*   _buffer;

public:
    LinearAllocator() noexcept;
    LinearAllocator(usize capacity) noexcept;
    LinearAllocator(LinearAllocator&& rhs) noexcept;
    LinearAllocator& operator=(LinearAllocator&& rhs) noexcept;
    ~LinearAllocator() noexcept;

    void* allocate(usize size, u16 alignment) noexcept;
    usize allocate_handle(usize size, u16 alignment) noexcept;
    ReallocReturn reallocate(void* addr, usize new_size, u16 alignment) noexcept;
    ReallocReturnHandle reallocate_handle(usize handle, usize new_size, u16 alignment) noexcept;
    void* handle_to_ptr(usize handle) const noexcept;
    usize ptr_to_handle(void* ptr) const noexcept;
    void clear() noexcept;
    void free(void* addr) noexcept {}
    void free_handle(usize handle) noexcept {}

    constexpr u8* begin() noexcept { return _buffer; }
    constexpr u8* data() noexcept { return _buffer; }
    constexpr u8* end() noexcept { return _buffer + _count; }
    constexpr usize count() const noexcept { return _count; }
    constexpr usize capacity() const noexcept { return _capacity; }
    // const counterparts
    const u8* begin() const noexcept { return _buffer; }
    const u8* data() const noexcept { return _buffer; }
    const u8* end() const noexcept { return _buffer + _count; }

private:
    void resize(usize new_capacity) noexcept;
};

} // sf
