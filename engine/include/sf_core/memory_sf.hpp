#pragma once
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/utils.hpp"

// NOTE: temp
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

struct ArenaAllocator {
    u64 buffer_size;
    u8* buffer;
    u64 offset;

    ArenaAllocator();
    ArenaAllocator(u64 buffer_size);
    ArenaAllocator(ArenaAllocator&& rhs) noexcept;
    ~ArenaAllocator();

    template<typename T, typename... Args>
    T* allocate(Args&&... args)
    {
        constexpr usize sizeo_of_T = sizeof(T);
        constexpr usize align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        // Same as (ptr % alignment) but faster as 'alignemnt' is a power of two
        u32 padding = reinterpret_cast<usize>(buffer + offset) & (align_of_T - 1);
        if (offset + padding + sizeo_of_T > buffer_size) {
            this->reallocate(buffer_size * 2);
        }
        T* ptr_to_return = reinterpret_cast<T*>(buffer + offset + padding);
        offset += padding + sizeo_of_T;
        new (ptr_to_return) T(std::forward<Args>(args)...);
        return ptr_to_return;
    }

    void reallocate(u64 new_size);
};

template<typename T>
struct PoolAllocator {
    u64 buffer_size;
    u8* buffer;
    u64 offset;
    static u16 ref_count;

    using value_type    = T;
    using pointer       = T*;

    PoolAllocator(u64 buffer_size_)
        : buffer_size{ buffer_size_ }
        , buffer{ static_cast<u8*>(sf_mem_alloc(buffer_size)) }
        , offset{ 0 }
    {}

    PoolAllocator(PoolAllocator<T>&& rhs) noexcept
        : buffer_size{ rhs.buffer_size }
        , buffer{ rhs.buffer }
        , offset{ rhs.offset }
    {
        rhs.buffer_size = 0;
        rhs.buffer = nullptr;
        rhs.offset = 0;
    }

    PoolAllocator<T>& operator=(PoolAllocator<T>&& rhs) noexcept
    {
        buffer_size = rhs.buffer_size;
        buffer = rhs.buffer;
        offset = rhs.offset;
        rhs.buffer_size = 0;
        rhs.buffer = nullptr;
        rhs.offset = 0;

        return *this;
    }

    PoolAllocator(const PoolAllocator<T>& rhs) = delete;
    PoolAllocator<T>& operator=(const PoolAllocator<T>& rhs) = delete;

    ~PoolAllocator()
    {
        if (buffer) {
            sf_mem_free(buffer, buffer_size);
            buffer = nullptr;
        }
    }

    T* allocate(u64 count)
    {
        constexpr usize sizeo_of_T = sizeof(T);
        constexpr usize align_of_T = alignof(T);

        static_assert(is_power_of_two(align_of_T) && "should be power of 2");

        // Same as (ptr % alignment) but faster as 'alignemnt' is a power of two
        u32 padding = reinterpret_cast<usize>(buffer + offset) & (align_of_T - 1);
        if (offset + padding + sizeo_of_T * count > buffer_size) {
            this->reallocate(buffer_size * 2);
        }
        T* ptr_to_return = reinterpret_cast<T*>(buffer + offset + padding);
        offset += padding + sizeo_of_T * count;
        return ptr_to_return;
    }

    void deallocate(T* ptr, u64 count)
    {}

    template<typename ...Args>
    void construct(T* ptr, Args&&... args)
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    void reallocate(u64 new_size)
    {
        buffer_size = new_size;
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_size));
        sf::platform_mem_copy(new_buffer, buffer, offset);
        sf_mem_free(buffer, buffer_size);
        buffer = new_buffer;
    }
};

} // sf
