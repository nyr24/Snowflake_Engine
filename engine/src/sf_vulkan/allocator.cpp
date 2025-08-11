#include "sf_vulkan/allocator.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {

void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);

    for (auto& list : allocator_state->lists) {
        void* block = list.allocate_block(alloc_bytes, alignment);
        if (block) {
            return block;
        }
    }

    return nullptr;
}

void vk_free_fn(void* user_data, void* node_to_be_removed) {
    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);

    for (auto& list : allocator_state->lists) {
        if (is_address_in_range(list.begin(), list.total_size(), node_to_be_removed)) {
            list.free_block(node_to_be_removed);
        }
    }
}

void* vk_realloc_fn(void* user_data, void* p_original, usize realloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);
    // alloc
    if (!p_original) {
        return allocator_state->callbacks.pfnAllocation(user_data, realloc_bytes, alignment, vk_alloc_scope);
    }
    // free
    else if (realloc_bytes == 0) {
        allocator_state->callbacks.pfnFree(user_data, p_original);
        return nullptr;
    }
    // free + alloc
    else {
        allocator_state->callbacks.pfnFree(user_data, p_original);
        return allocator_state->callbacks.pfnAllocation(user_data, realloc_bytes, alignment, vk_alloc_scope);
    }
}

void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_alloc of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_free of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

// // Utilities for moving between nodes
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_byte_amount_copy(VulkanAllocatorBlockNode* node, u32 byte_amount) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + byte_amount);
// }
//
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_before_header_copy(VulkanAllocatorBlockNode* node) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) - sizeof(VulkanAllocatorBlockNode));
// }
//
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_header_copy(VulkanAllocatorBlockNode* node) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode));
// }
//
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_padding_copy(VulkanAllocatorBlockNode* node) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding);
// }
//
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_byte_used_copy(VulkanAllocatorBlockNode* node) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding + node->byte_used);
// }
//
// VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_byte_size_copy(VulkanAllocatorBlockNode* node) noexcept {
//     return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding + node->byte_size);
// }
//
// void VulkanAllocatorBlockNode::step_byte_amount_inplace(VulkanAllocatorBlockNode* node, u32 byte_amount) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + byte_amount);
// }
//
// void VulkanAllocatorBlockNode::step_before_header_inplace(VulkanAllocatorBlockNode* node) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) - sizeof(VulkanAllocatorBlockNode));
// }
//
// void VulkanAllocatorBlockNode::step_after_header_inplace(VulkanAllocatorBlockNode* node) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode));
// }
//
// void VulkanAllocatorBlockNode::step_after_padding_inplace(VulkanAllocatorBlockNode* node) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding);
// }
//
// void VulkanAllocatorBlockNode::step_after_byte_used_inplace(VulkanAllocatorBlockNode* node) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->byte_used);
// }
//
// void VulkanAllocatorBlockNode::step_after_byte_size_inplace(VulkanAllocatorBlockNode* node) noexcept {
//     node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding + node->byte_size);
// }
//
// // VulkanAllocatorBlockList
// VulkanAllocatorBlockList::VulkanAllocatorBlockList(u32 capacity) noexcept
//     : _mem_have{ capacity }
//     , _mem_used{0}
//     , _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
//     , _head_offset{0}
//     , _tail_offset{0}
// {
//     VulkanAllocatorBlockNode* head = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer);
//     head->zero_node();
// }
//
// VulkanAllocatorBlockList::~VulkanAllocatorBlockList() noexcept {
//     if (_buffer) {
//         sf_mem_free(_buffer, alignof(u8));
//         _buffer = nullptr;
//     }
// }
//
// bool VulkanAllocatorBlockList::is_address_in_range(void* addr) noexcept {
//     return addr >= (_buffer + _head_offset) && addr < (_buffer + _tail_offset);
// }
//
// void* VulkanAllocatorBlockList::insert(u32 bytes_need, u32 alignment) noexcept {
//     if (_mem_used == 0) {
//         return insert_first(bytes_need, alignment);
//     }
//
//     void* result = nullptr;
//
//     if (_mem_have - (_tail_offset + last_node_ptr()->size_with_header()) >= (sizeof(VulkanAllocatorBlockNode) + alignment + bytes_need)) {
//         if ((result = insert_end(bytes_need, alignment))) {
//             return result;
//         }
//     }
//
//     if (!result && (_mem_have - _head_offset >= sizeof(VulkanAllocatorBlockNode) + alignment + bytes_need)) {
//         if ((result = insert_begin(bytes_need, alignment))) {
//             return result;
//         }
//     }
//
//     if (!result) {
//         if ((result = insert_search(bytes_need, alignment))) {
//             return result;
//         }
//     }
//
//     return result;
// }
//
// void* VulkanAllocatorBlockList::insert_first(u32 bytes_need, u32 alignment) noexcept {
//     VulkanAllocatorBlockNode* new_node_header_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer);
//     u32 new_node_padding = sf_calc_padding(new_node_header_ptr + 1, alignment);
//
//     new_node_header_ptr->padding = new_node_padding;
//     new_node_header_ptr->padding = 0;
//     new_node_header_ptr->byte_used = bytes_need;
//     new_node_header_ptr->byte_size = new_node_padding + bytes_need;
//     new_node_header_ptr->byte_size = bytes_need;
//
//     _mem_used += new_node_header_ptr->size_with_header();
//
//     return VulkanAllocatorBlockNode::step_after_padding_copy(new_node_header_ptr);
//     // return VulkanAllocatorBlockNode::step_after_header_copy(new_node_header_ptr);
// }
//
// // TODO: rework
// void* VulkanAllocatorBlockList::insert_begin(u32 bytes_need, u32 padding) noexcept {
//     VulkanAllocatorBlockNode* new_node_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _head_offset - bytes_need - padding);
//
//     new_node_ptr->padding = padding;
//     new_node_ptr->byte_used = bytes_need;
//     new_node_ptr->byte_size = bytes_need + padding;
//
//     void* return_ptr = VulkanAllocatorBlockNode::step_after_padding_copy(new_node_ptr);
//     _head_offset -= (new_node_ptr->size_with_header());
//     _mem_used += new_node_ptr->size_with_header();
//
//     return return_ptr;
// }
//
// void* VulkanAllocatorBlockList::insert_end(u32 bytes_need, u32 alignment) noexcept {
//     VulkanAllocatorBlockNode* last_node_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _tail_offset);
//     u32 padding_before_new_node_header = sf_calc_padding(VulkanAllocatorBlockNode::step_after_byte_size_copy(last_node_ptr), alignof(VulkanAllocatorBlockNode));
//     last_node_ptr->byte_size += padding_before_new_node_header;
//     VulkanAllocatorBlockNode* new_node_header_ptr = VulkanAllocatorBlockNode::step_after_byte_size_copy(last_node_ptr);
//     u32 padding_before_new_node = sf_calc_padding(new_node_header_ptr + 1, alignment);
//     bool have_place = (_mem_have - (_tail_offset + last_node_ptr->size_with_header()) >= (sizeof(VulkanAllocatorBlockNode) + padding_before_new_node + bytes_need));
//
//     if (!have_place) {
//         return nullptr;
//     }
//
//     new_node_header_ptr->padding = padding_before_new_node;
//     new_node_header_ptr->padding = 0;
//     new_node_header_ptr->byte_used = bytes_need;
//     new_node_header_ptr->byte_size = bytes_need + padding_before_new_node;
//     // new_node_header_ptr->byte_size = bytes_need;
//
//     void* return_ptr = VulkanAllocatorBlockNode::step_after_header_copy(new_node_header_ptr);
//
//     _tail_offset += last_node_ptr->size_with_header();
//     _mem_used += new_node_header_ptr->size_with_header();
//
//     return return_ptr;
// }
//
// // function assumes that there is NO empty space before _buffer + _head_offset and after _buffer + _tail_offset
// // check is inside "insert()"
// void* VulkanAllocatorBlockList::insert_search(u32 bytes_need, u32 alignment) noexcept {
//     VulkanAllocatorBlockNode* curr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _head_offset);
//     VulkanAllocatorBlockNode* end = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _tail_offset);
//
//     while (curr != end) {
//         if (curr->byte_used == 0 && curr->byte_size >= bytes_need) {
//             VulkanAllocatorBlockNode* maybe_new_node = VulkanAllocatorBlockNode::step_after_header_copy(curr);
//             u32 maybe_new_node_padding = reinterpret_cast<usize>(maybe_new_node) % alignment;
//
//             if (curr->byte_size >= bytes_need + maybe_new_node_padding) {
//                 curr->padding = maybe_new_node_padding;
//                 curr->byte_used = bytes_need;
//                 _mem_used += maybe_new_node_padding + bytes_need;
//                 return maybe_new_node;
//             }
//         }
//         else if (curr->byte_size - curr->byte_used >= bytes_need + sizeof(VulkanAllocatorBlockNode)) {
//             VulkanAllocatorBlockNode* maybe_new_node = VulkanAllocatorBlockNode::step_after_byte_used_copy(curr);
//             VulkanAllocatorBlockNode::step_after_header_inplace(maybe_new_node);
//             u32 maybe_new_node_padding = reinterpret_cast<usize>(maybe_new_node) % alignment;
//
//             if (curr->byte_size - curr->byte_used >= sizeof(VulkanAllocatorBlockNode) + maybe_new_node_padding + bytes_need) {
//                 VulkanAllocatorBlockNode* new_node_header = VulkanAllocatorBlockNode::step_before_header_copy(maybe_new_node);
//                 new_node_header->padding = maybe_new_node_padding;
//                 new_node_header->byte_used = bytes_need;
//
//                 // byte_size should give proper offset to the next node previously pointed by 'curr' to not break a chain
//                 new_node_header->byte_size = curr->byte_size - curr->byte_used - sizeof(VulkanAllocatorBlockNode);
//
//                 // shrink item 'curr' points to
//                 curr->byte_size = curr->byte_used;
//
//                 _mem_used += maybe_new_node_padding + bytes_need;
//
//                 return maybe_new_node;
//             }
//         }
//
//         // go to the next node
//         VulkanAllocatorBlockNode::step_after_byte_size_inplace(curr);
//     }
//
//     return nullptr;
// }
//
// void VulkanAllocatorBlockList::remove(void* node_to_be_removed) noexcept {
//     VulkanAllocatorBlockNode* header = VulkanAllocatorBlockNode::step_before_header_copy(static_cast<VulkanAllocatorBlockNode*>(node_to_be_removed));
//     header->padding = 0;
//     header->byte_used = 0;
// }

} // sf
