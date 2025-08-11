#pragma once

#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include <utility>

namespace sf {
u32 platform_get_mem_page_size();

struct LinearAllocator {
private:
    u32 _capacity;
    u32 _count;
    u8* _buffer;

public:
    LinearAllocator() noexcept;
    LinearAllocator(u32 capacity) noexcept;
    LinearAllocator(LinearAllocator&& rhs) noexcept;
    ~LinearAllocator() noexcept;

    u32 allocate(u32 size, u16 alignment) noexcept;
    void reallocate(u32 new_capacity) noexcept;
    void* get_with_handle(u32 handle) const noexcept;
    bool is_valid_handle(u32 handle) const noexcept;

    // returns handle (relative address to start of the buffer)
    template<typename T, typename... Args>
    u32 allocate(Args&&... args) noexcept
    {
        static_assert(is_power_of_two(alignof(T)) && "should be power of 2");

        u32 padding = sf_calc_padding(_buffer + _count, alignof(T));
        if (_count + padding + sizeof(T) > _capacity) {
            reallocate(_capacity * 2);
        }

        u32 handle_to_return = _count + padding;
        _count += padding + sizeof(T);

        construct_at<T, Args...>(reinterpret_cast<T*>(_buffer + handle_to_return), std::forward<Args>(args)...);

        return handle_to_return;
    }

    template<typename T>
    T* get_with_handle(u32 handle) const noexcept {
        if (!is_valid_handle(handle)) {
            return nullptr;
        }

        return reinterpret_cast<T*>(_buffer + handle);
    }

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
    template<typename T, typename ...Args>
    void construct_at(T* ptr, Args&&... args) noexcept
    {
        sf_mem_place(ptr, args...);
    }
};

} // sf
