#pragma once
#include "sf_core/defines.hpp"
#include "sf_core/types.hpp"
#include "sf_platform/platform_mem_alloc_templated.hpp"
#include <new>
#include <utility>

namespace sf {
enum MemoryTag {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_ARRAY_LINKED_LIST,
    MEMORY_TAG_HASHMAP,
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
template<typename T, bool should_align>
T* sf_mem_alloc_typed(u64 count) {
    return platform_mem_alloc_typed<T, should_align>(count);
}

template<typename T, bool should_align>
void sf_mem_free_typed(T* block) {
    if constexpr (should_align) {
        ::operator delete(block, static_cast<std::align_val_t>(alignof(T)), std::nothrow);
    } else {
        ::operator delete(block, std::nothrow);
    }
}

template<typename T, typename... Args>
T* sf_mem_construct(Args&&... args) {
    return new (std::nothrow) T(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T* sf_mem_place(T* ptr, Args&&... args) {
    return new (ptr) T(std::forward<Args>(args)...);
}

// non-templated versions of memory functions (needed for void*)
SF_EXPORT void* sf_mem_alloc(usize byte_size, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_free(void* block, usize byte_size = 0, u16 alignment = 0, MemoryTag tag = MemoryTag::MEMORY_TAG_UNKNOWN);
SF_EXPORT void  sf_mem_set(void* block, usize byte_size, i32 value);
SF_EXPORT void  sf_mem_zero(void* block, usize byte_size);
SF_EXPORT void  sf_mem_copy(void* dest, void* src, usize byte_size);
SF_EXPORT void  sf_mem_move(void* dest, void* src, usize byte_size);
SF_EXPORT bool  sf_mem_cmp(void* first, void* second, usize byte_size);
SF_EXPORT bool  sf_str_cmp(const char* first, const char* second);
SF_EXPORT char* get_memory_usage_str();

} // sf
