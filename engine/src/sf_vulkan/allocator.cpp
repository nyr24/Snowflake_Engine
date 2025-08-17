// #include "sf_vulkan/types.hpp"
// #include "sf_vulkan/allocator.hpp"
// #include "sf_core/logger.hpp"
// #include <vulkan/vulkan_core.h>

// TODO: custom allocator

namespace sf {

// void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
//     VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);
//
//     for (auto& list : allocator_state->lists) {
//         void* block = list.allocate_block(alloc_bytes, alignment);
//         if (block) {
//             return block;
//         }
//     }
//
//     return nullptr;
// }
//
// void vk_free_fn(void* user_data, void* node_to_be_removed) {
//     VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);
//
//     for (auto& list : allocator_state->lists) {
//         if (is_address_in_range(list.begin(), list.total_size(), node_to_be_removed)) {
//             list.free_block(node_to_be_removed);
//         }
//     }
// }
//
// void* vk_realloc_fn(void* user_data, void* p_original, usize realloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
//     VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);
//     // alloc
//     if (!p_original) {
//         return allocator_state->callbacks.pfnAllocation(user_data, realloc_bytes, alignment, vk_alloc_scope);
//     }
//     // free
//     else if (realloc_bytes == 0) {
//         allocator_state->callbacks.pfnFree(user_data, p_original);
//         return nullptr;
//     }
//     // free + alloc
//     else {
//         allocator_state->callbacks.pfnFree(user_data, p_original);
//         return allocator_state->callbacks.pfnAllocation(user_data, realloc_bytes, alignment, vk_alloc_scope);
//     }
// }
//
// void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
//     LOG_INFO("vk_intern_alloc of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
// }
//
// void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
//     LOG_INFO("vk_intern_free of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
// }

} // sf
