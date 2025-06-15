#pragma once
#include "sf_core/types.hpp"
#include "sf_platform/platform.hpp"

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

SF_EXPORT void* sf_mem_alloc(u64 byte_size, MemoryTag tag);
SF_EXPORT void  sf_mem_free(void* block, u64 byte_size, MemoryTag tag);
SF_EXPORT void  sf_mem_zero(void* block, u64 size);
SF_EXPORT void  sf_mem_copy(void* dest, const void* source, u64 size);
SF_EXPORT void  sf_mem_set(void* dest, i32 value, u64 size);
SF_EXPORT i8*   get_memory_usage_str();
} // sf
