#include "sf_vulkan/allocator.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

VulkanAllocatorState::VulkanAllocatorState(u32 capacity)
    : _capacity{capacity}
{}

void VulkanAllocatorState::grow_capacity(u32 new_capacity, usize alignment) {
    u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity, alignment));
    sf_mem_copy(new_buffer, _buffer, _count);
    sf_mem_free(_buffer, _capacity);
    _capacity = new_capacity;
    _buffer = new_buffer;
}


void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_alloc_fn system scope: {}", static_cast<i32>(vk_alloc_scope));

    VulkanAllocatorState* state = static_cast<VulkanAllocatorState*>(user_data);

    usize padding_bytes = reinterpret_cast<usize>(state->_buffer + state->_count) % alignment;
    if (state->_count + padding_bytes + alloc_bytes > state->_capacity) {
        state->grow_capacity(state->_capacity * 2, alignment);
    }

    void* ptr_to_return = state->_buffer + state->_count + padding_bytes;
    state->_count += padding_bytes + alloc_bytes;

    return ptr_to_return;
}

void vk_free_fn(void* state, void* memory) {
    LOG_INFO("vk_free call, ptr: {}", memory);
}

void* vk_realloc_fn(void* state, void* p_original, usize realloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_realloc_fn call, of size bytes: {}, alignment: {}", realloc_bytes, alignment);
    return p_original;
}

void vk_intern_alloc_notification(void* state, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_alloc of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

void vk_intern_free_notification(void* state, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_free of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

} // sf
