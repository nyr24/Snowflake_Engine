#pragma once

#include "sf_core/defines.hpp"
#include <vulkan/vulkan_core.h>
// #include "sf_allocators/free_list_allocator.hpp"
// #include <list>

// limitation: we can't reallocate memory which is used by vulkan
// because it would invalidate addresses

namespace sf {

void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope);
void vk_free_fn(void* user_data, void* node_to_be_removed);
void* vk_realloc_fn(void* user_data, void* p_original, usize size, usize alignment, VkSystemAllocationScope vk_alloc_scope);
void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);
void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);

} // sf
