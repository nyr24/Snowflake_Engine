#pragma once

#include "sf_core/defines.hpp"
#include <list>
#include <vulkan/vulkan_core.h>

// limitation: we can't reallocate our memory which used by vulkan
// because it would invalidate addresses

namespace sf {

struct VulkanAllocatorBlockNode {
    // NOTE: padding + byte_used not always will be equal to byte_size
    u32 byte_used;
    u32 padding;
    u32 byte_size;

    constexpr bool is_empty() const noexcept { return padding == 0 && byte_size == 0; }
    constexpr void zero_node() noexcept { padding = 0; byte_size = 0; byte_used = 0; }
    constexpr void free_memory() noexcept { padding = 0; byte_used = 0; }

    static void step_before_header_inplace(VulkanAllocatorBlockNode* node) noexcept;
    static void step_after_header_inplace(VulkanAllocatorBlockNode* node) noexcept;
    static void step_after_padding_inplace(VulkanAllocatorBlockNode* node) noexcept;
    static void step_after_byte_used_inplace(VulkanAllocatorBlockNode* node) noexcept;
    static void step_after_byte_size_inplace(VulkanAllocatorBlockNode* node) noexcept;
    static VulkanAllocatorBlockNode* step_after_header_copy(VulkanAllocatorBlockNode* node) noexcept;
    static VulkanAllocatorBlockNode* step_before_header_copy(VulkanAllocatorBlockNode* node) noexcept;
    static VulkanAllocatorBlockNode* step_after_padding_copy(VulkanAllocatorBlockNode* node) noexcept;
    static VulkanAllocatorBlockNode* step_after_byte_used_copy(VulkanAllocatorBlockNode* node) noexcept;
    static VulkanAllocatorBlockNode* step_after_byte_size_copy(VulkanAllocatorBlockNode* node) noexcept;
};

struct VulkanAllocatorBlockList {
    u32 _mem_have;
    u32 _mem_used;
    u8* _buffer;
    // offset to the first used Node header
    u32 _head_offset;
    // offset to the last used Node header
    u32 _tail_offset;

public:
    explicit VulkanAllocatorBlockList(u32 capacity) noexcept;
    ~VulkanAllocatorBlockList() noexcept;

    constexpr bool is_full() const noexcept { return _mem_have == _mem_used; }
    constexpr u32 remain_memory() const noexcept { return _mem_have - _mem_used; }
    bool is_address_in_range(void* addr) noexcept;
    void* insert(u32 byte_size, u32 alignment) noexcept;
    void remove(void* node_to_be_removed) noexcept;

private:
    void* insert_begin(u32 bytes_need, u32 padding) noexcept;
    void* insert_end(u32 bytes_need, u32 padding) noexcept;
    void* insert_search(u32 bytes_need, u32 alignment) noexcept;
};

void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope);
void vk_free_fn(void* user_data, void* node_to_be_removed);
void* vk_realloc_fn(void* user_data, void* p_original, usize size, usize alignment, VkSystemAllocationScope vk_alloc_scope);
void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);
void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope);

struct VulkanAllocator {
    std::list<VulkanAllocatorBlockList>     blocks;
    VkAllocationCallbacks                   callbacks;
};

} // sf
