#pragma once
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/utils.hpp"
#include <concepts>

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
SF_EXPORT void* sf_mem_alloc(usize byte_size, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_free(void* block, usize byte_size, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_zero(void* block, usize size);
SF_EXPORT void  sf_mem_copy(void* dest, const void* source, usize size);
SF_EXPORT void  sf_mem_set(void* dest, i32 value, usize size);
SF_EXPORT i8*   get_memory_usage_str();

// custom allocators
template<typename A, typename T, typename ...Args>
concept MonoTypeAllocator = requires(A a, Args... args) {
    { a.allocate(std::declval<u64>()) } -> std::same_as<T*>;
    { a.deallocate(std::declval<T*>(), std::declval<u64>()) } -> std::same_as<void>;
    { a.construct(std::declval<T*>(), std::forward<Args>(args)...) } -> std::same_as<void>;
    { a.allocate_and_construct(std::forward<Args>(args)...) } -> std::same_as<T*>;
    { a.reallocate(std::declval<u64>()) } -> std::same_as<void>;
    { a.begin() } -> std::same_as<T*>;
    { a.end() } -> std::same_as<T*>;
    { a.ptr_offset(std::declval<u64>()) } -> std::same_as<T*>;
    { a.ptr_offset_val(std::declval<u64>()) } -> std::same_as<T&>;
    { a.len() } -> std::same_as<usize>;
    { a.capacity() } -> std::same_as<usize>;
};

// TODO:
template<typename A, typename ...Args>
concept PolyTypeAllocator = true;

// struct ArenaAllocator {
//     usize _capacity;
//     u8* _buffer;
//     usize _len;
//
//     ArenaAllocator();
//     ArenaAllocator(usize _capacity);
//     ArenaAllocator(ArenaAllocator&& rhs) noexcept;
//     ~ArenaAllocator();
//
//     template<typename T, typename... Args>
//     T* allocate(Args&&... args)
//     {
//         constexpr usize sizeo_of_T = sizeof(T);
//         constexpr usize align_of_T = alignof(T);
//
//         static_assert(is_power_of_two(align_of_T) && "should be power of 2");
//
//         // Same as (ptr % alignment) but faster as 'alignemnt' is a power of two
//         u32 padding_bytes = reinterpret_cast<usize>(_buffer + _len) & (align_of_T - 1);
//         if (_len + padding_bytes + sizeo_of_T > _capacity) {
//             this->reallocate(_capacity * 2);
//         }
//         T* ptr_to_return = reinterpret_cast<T*>(_buffer + _len + padding_bytes);
//         _len += padding_bytes + sizeo_of_T;
//         new (ptr_to_return) T(std::forward<Args>(args)...);
//         return ptr_to_return;
//     }
//
//     void deallocate() {}
//     void reallocate(usize new_size);
// };

template<typename T>
struct PoolAllocator {
private:
    usize   _capacity;
    usize   _len;
    u8*     _buffer;

public:
    using value_type    = T;
    using pointer       = T*;

    static_assert(is_power_of_two(alignof(T)) && "should be power of 2");

    PoolAllocator(u64 count)
        : _capacity{ sizeof(T) * count }
        , _len{ 0 }
        , _buffer{ static_cast<u8*>(sf_mem_alloc(_capacity, alignof(T))) }
    {}

    PoolAllocator(PoolAllocator<T>&& rhs) noexcept
        : _capacity{ rhs._capacity }
        , _len{ rhs._len }
        , _buffer{ rhs._buffer }
    {
        rhs._capacity = 0;
        rhs._len = 0;
        rhs._buffer = nullptr;
    }

    PoolAllocator<T>& operator=(PoolAllocator<T>&& rhs) noexcept
    {
        _capacity = rhs._capacity;
        _len = rhs._len;
        _buffer = rhs._buffer;
        rhs._capacity = 0;
        rhs._len = 0;
        rhs._buffer = nullptr;

        return *this;
    }

    PoolAllocator(const PoolAllocator<T>& rhs) = delete;
    PoolAllocator<T>& operator=(const PoolAllocator<T>& rhs) = delete;

    ~PoolAllocator()
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity, alignof(T));
            _buffer = nullptr;
        }
    }

    T* allocate(u64 count)
    {
        usize need_memory = sizeof(T) * static_cast<usize>(count);
        usize padding = reinterpret_cast<usize>(_buffer + _len) & (alignof(T) - 1);
        usize have_memory = _capacity - (_len + padding);

        if (have_memory < need_memory) {
            this->reallocate(_capacity * 2);
        }

        T* return_memory = reinterpret_cast<T*>(_buffer + _len + padding);
        _len += padding + need_memory;

        return return_memory;
    }

    void deallocate(T* ptr, u64 count) noexcept
    {}

    template<typename ...Args>
    void construct(T* ptr, Args&&... args) noexcept
    {
        ::new (ptr) T(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    T* allocate_and_construct(Args&&... args)
    {
        T* place_ptr = allocate(1);
        construct(place_ptr, std::forward<Args>(args)...);
        return place_ptr;
    }

    void reallocate(usize new_capacity)
    {
        _capacity = new_capacity;
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity, alignof(T)));
        sf::platform_mem_copy(new_buffer, _buffer, _len);
        sf_mem_free(_buffer, _capacity);
        _buffer = new_buffer;
    }

    T* begin() noexcept { return reinterpret_cast<T*>(_buffer); }
    T* end() noexcept { return reinterpret_cast<T*>(_buffer + _len); }
    T* ptr_offset(usize ind) noexcept { return reinterpret_cast<T*>(_buffer) + ind; }
    T& ptr_offset_val(usize ind) noexcept { return *(ptr_offset(ind)); }
    usize len() noexcept { return _len / sizeof(T); }
    usize capacity() noexcept { return _capacity / sizeof(T); }
};

} // sf
