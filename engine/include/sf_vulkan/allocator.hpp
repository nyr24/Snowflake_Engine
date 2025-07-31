#pragma once

#include "sf_core/types.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

struct VulkanAllocatorState {
    u32 _capacity;
    u32 _count;
    u8* _buffer;

public:
    VulkanAllocatorState(u32 capacity);
    void grow_capacity(u32 new_capacity, usize alignment = 0);
};

void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope);
void vk_free_fn(void* user_data, void* memory);
void* vk_realloc_fn(void* user_data, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);
void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);

struct VulkanAllocator {
    VulkanAllocatorState    state;
    VkAllocationCallbacks   callbacks;
};

} // sf
