#pragma once
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/utils.hpp"
#include <concepts>

// NOTE: temp
#include <functional>
#include <iostream>

namespace sf {
enum MemoryTag {
    MEMORY_TAG_UNKNOWN,
    // MEMORY_TAG_ARRAY,
    // MEMORY_TAG_DARRAY,
    // MEMORY_TAG_DICT,
    // MEMORY_TAG_RING_QUEUE,
    // MEMORY_TAG_BST,
    // MEMORY_TAG_STRING,
    MEMORY_TAG_APPLICATION,
    MEMORY_TAG_JOB,
    MEMORY_TAG_TEXTURE,
    MEMORY_TAG_MATERIAL_INSTANCE,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_GAME,
    MEMORY_TAG_TRANSFORM,
    MEMORY_TAG_ENTITY,
    MEMORY_TAG_ENTITY_NODE,
    MEMORY_TAG_SCENE,
    MEMORY_TAG_MAX_TAGS
};

// templated versions of memory functions
template<typename T, typename... Args>
SF_EXPORT T* sf_mem_alloc(Args&&... args) {
    return platform_mem_alloc<T, Args...>(std::forward<Args>(args)...);
}

template<typename T>
SF_EXPORT void sf_mem_free(T* block) {
    return platform_mem_free<T>(block);
}

// non-templated versions of memory functions (void*)
SF_EXPORT void* sf_mem_alloc(u64 byte_size, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_free(void* block, u64 byte_size, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_zero(void* block, u64 size);
SF_EXPORT void  sf_mem_copy(void* dest, const void* source, u64 size);
SF_EXPORT void  sf_mem_set(void* dest, i32 value, u64 size);
SF_EXPORT i8*   get_memory_usage_str();

// custom allocators
template<typename T, typename U, typename ...Args>
concept Allocator = requires(T a, Args... args) {
    { a.allocate(100u) } -> std::same_as<U*>;
    { a.deallocate(reinterpret_cast<U*>(1), 100u) } -> std::same_as<void>;
    { a.construct(reinterpret_cast<U*>(1), 100u) } -> std::same_as<void>;
    { a.reallocate(100u) } -> std::same_as<void>;

    requires std::is_same_v<decltype(a.buffer), u8*>;
    requires std::is_same_v<decltype(a.capacity), u64>;
    requires std::is_same_v<decltype(a.len), u64>;
};

struct ArenaAllocator {
    u64 capacity;
    u8* buffer;
    u64 len;

    ArenaAllocator();
    ArenaAllocator(u64 capacity);
    ArenaAllocator(ArenaAllocator&& rhs) noexcept;
    ~ArenaAllocator();

    template<typename T, typename... Args>
    T* allocate(Args&&... args)
    {
        constexpr usize sizeo_of_T = sizeof(T);
        constexpr usize align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        // Same as (ptr % alignment) but faster as 'alignemnt' is a power of two
        u32 padding = reinterpret_cast<usize>(buffer + len) & (align_of_T - 1);
        if (len + padding + sizeo_of_T > capacity) {
            this->reallocate(capacity * 2);
        }
        T* ptr_to_return = reinterpret_cast<T*>(buffer + len + padding);
        len += padding + sizeo_of_T;
        new (ptr_to_return) T(std::forward<Args>(args)...);
        return ptr_to_return;
    }

    void deallocate() {}
    void reallocate(u64 new_size);
};

template<typename T>
struct PoolAllocator {
    // TODO: should be made private
    u64 capacity;
    u8* buffer;
    u64 len;

    using value_type    = T;
    using pointer       = T*;

    PoolAllocator(u64 capacity_)
        : capacity{ capacity_ }
        , buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
        , len{ 0 }
    {}

    PoolAllocator(PoolAllocator<T>&& rhs) noexcept
        : capacity{ rhs.capacity }
        , buffer{ rhs.buffer }
        , len{ rhs.len }
    {
        rhs.capacity = 0;
        rhs.buffer = nullptr;
        rhs.len = 0;
    }

    PoolAllocator<T>& operator=(PoolAllocator<T>&& rhs) noexcept
    {
        capacity = rhs.capacity;
        buffer = rhs.buffer;
        len = rhs.len;
        rhs.capacity = 0;
        rhs.buffer = nullptr;
        rhs.len = 0;

        return *this;
    }

    PoolAllocator(const PoolAllocator<T>& rhs) = delete;
    PoolAllocator<T>& operator=(const PoolAllocator<T>& rhs) = delete;

    ~PoolAllocator()
    {
        if (buffer) {
            sf_mem_free(buffer, capacity);
            buffer = nullptr;
        }
    }

    T* allocate(u64 count)
    {
        constexpr usize sizeo_of_T = sizeof(T);
        constexpr usize align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        u64 need_memory = sizeo_of_T * count;
        u32 padding = reinterpret_cast<usize>(buffer + len) & (align_of_T - 1);
        u64 have_memory = capacity - (len + padding);

        if (have_memory < need_memory) {
            this->reallocate(capacity * 2);
        }

        len += padding + need_memory;
        return reinterpret_cast<T*>(buffer + len + padding);
    }

    void deallocate(T* ptr, u64 count)
    {}

    template<typename ...Args>
    void construct(T* ptr, Args&&... args)
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    void reallocate(u64 new_capacity)
    {
        capacity = new_capacity;
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity));
        sf::platform_mem_copy(new_buffer, buffer, len);
        sf_mem_free(buffer, capacity);
        buffer = new_buffer;
    }
};

} // sf
