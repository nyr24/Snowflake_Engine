#include "sf_vulkan/allocator.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_platform/platform.hpp"

namespace sf {

// Utilities for moving between nodes
VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_before_header_copy(VulkanAllocatorBlockNode* node) noexcept {
    return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) - sizeof(VulkanAllocatorBlockNode));
}

VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_header_copy(VulkanAllocatorBlockNode* node) noexcept {
    return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode));
}

VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_padding_copy(VulkanAllocatorBlockNode* node) noexcept {
    return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding);
}

VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_byte_used_copy(VulkanAllocatorBlockNode* node) noexcept {
    return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding + node->byte_used);
}

VulkanAllocatorBlockNode* VulkanAllocatorBlockNode::step_after_byte_size_copy(VulkanAllocatorBlockNode* node) noexcept {
    return reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding + node->byte_size);
}

void VulkanAllocatorBlockNode::step_before_header_inplace(VulkanAllocatorBlockNode* node) noexcept {
    node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) - sizeof(VulkanAllocatorBlockNode));
}

void VulkanAllocatorBlockNode::step_after_header_inplace(VulkanAllocatorBlockNode* node) noexcept {
    node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode));
}

void VulkanAllocatorBlockNode::step_after_padding_inplace(VulkanAllocatorBlockNode* node) noexcept {
    node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->padding);
}

void VulkanAllocatorBlockNode::step_after_byte_used_inplace(VulkanAllocatorBlockNode* node) noexcept {
    node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->byte_used);
}

void VulkanAllocatorBlockNode::step_after_byte_size_inplace(VulkanAllocatorBlockNode* node) noexcept {
    node = reinterpret_cast<VulkanAllocatorBlockNode*>(reinterpret_cast<u8*>(node) + sizeof(VulkanAllocatorBlockNode) + node->byte_size);
}

// VulkanAllocatorBlockList
VulkanAllocatorBlockList::VulkanAllocatorBlockList(u32 capacity) noexcept
    : _mem_have{ capacity }
    , _mem_used{0}
    , _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    , _head_offset{0}
    , _tail_offset{0}
{
    VulkanAllocatorBlockNode* head = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer);
    head->zero_node();
}

VulkanAllocatorBlockList::~VulkanAllocatorBlockList() noexcept {
    if (_buffer) {
        sf_mem_free(_buffer, alignof(u8));
        _buffer = nullptr;
    }
}

bool VulkanAllocatorBlockList::is_address_in_range(void* addr) noexcept {
    return addr >= (_buffer + _head_offset) && addr < (_buffer + _tail_offset);
}

void* VulkanAllocatorBlockList::insert(u32 bytes_need, u32 alignment) noexcept {
    SF_ASSERT_MSG((_mem_have - _mem_used) >= (bytes_need + alignment), "Shound have enough memory for inserting object of this size");

    VulkanAllocatorBlockNode* last_node_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _tail_offset);
    u32 last_node_size_with_header = sizeof(VulkanAllocatorBlockNode) + last_node_ptr->byte_size;
    u32 end_padding = (reinterpret_cast<usize>(last_node_ptr) + last_node_size_with_header + sizeof(VulkanAllocatorBlockNode)) % alignment;
    bool end_have_place = (_mem_have - (_tail_offset + last_node_size_with_header) >= (sizeof(VulkanAllocatorBlockNode) + end_padding + bytes_need));

    if (end_have_place) {
        return insert_end(bytes_need, end_padding);
    } else {
        u32 begin_padding = (reinterpret_cast<usize>(_buffer + _head_offset - bytes_need)) % alignment;
        bool begin_have_place = _head_offset >= (sizeof(VulkanAllocatorBlockNode) + bytes_need + begin_padding);

        if (begin_have_place) {
            return insert_begin(bytes_need, begin_padding);
        } else {
            return insert_search(bytes_need, alignment);
        }
    }
}

void* VulkanAllocatorBlockList::insert_begin(u32 bytes_need, u32 padding) noexcept {
    VulkanAllocatorBlockNode* header_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _head_offset - bytes_need - padding);
    header_ptr->byte_size = bytes_need + padding;
    header_ptr->byte_used = bytes_need;
    header_ptr->padding = padding;

    void* return_ptr = VulkanAllocatorBlockNode::step_after_padding_copy(header_ptr);
    _head_offset -= (header_ptr->byte_size + sizeof(VulkanAllocatorBlockNode));
    _mem_used += sizeof(VulkanAllocatorBlockNode) + header_ptr->byte_size;

    return return_ptr;
}

void* VulkanAllocatorBlockList::insert_end(u32 bytes_need, u32 padding) noexcept {
    VulkanAllocatorBlockNode* last_node_header_ptr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _tail_offset);
    VulkanAllocatorBlockNode* new_node_header_ptr = VulkanAllocatorBlockNode::step_after_byte_size_copy(last_node_header_ptr);

    new_node_header_ptr->byte_size = bytes_need + padding;
    new_node_header_ptr->byte_used = bytes_need;
    new_node_header_ptr->padding = padding;

    void* return_ptr = VulkanAllocatorBlockNode::step_after_padding_copy(new_node_header_ptr);
    _tail_offset += sizeof(VulkanAllocatorBlockNode) + last_node_header_ptr->byte_size;
    _mem_used += sizeof(VulkanAllocatorBlockNode) + new_node_header_ptr->byte_size;

    return return_ptr;
}

// function assumes that there is NO empty space before _buffer + _head_offset and after _buffer + _tail_offset
// check is inside "insert()"
void* VulkanAllocatorBlockList::insert_search(u32 bytes_need, u32 alignment) noexcept {
    VulkanAllocatorBlockNode* curr = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _head_offset);
    VulkanAllocatorBlockNode* end = reinterpret_cast<VulkanAllocatorBlockNode*>(_buffer + _tail_offset);

    while (curr != end) {
        if (curr->byte_used == 0 && curr->byte_size >= bytes_need) {
            VulkanAllocatorBlockNode* maybe_new_node = VulkanAllocatorBlockNode::step_after_header_copy(curr);
            u32 maybe_new_node_padding = reinterpret_cast<usize>(maybe_new_node) % alignment;

            if (curr->byte_size >= bytes_need + maybe_new_node_padding) {
                curr->padding = maybe_new_node_padding;
                curr->byte_used = bytes_need;
                _mem_used += maybe_new_node_padding + bytes_need;
                return maybe_new_node;
            }
        }
        else if (curr->byte_size - curr->byte_used >= bytes_need + sizeof(VulkanAllocatorBlockNode)) {
            VulkanAllocatorBlockNode* maybe_new_node = VulkanAllocatorBlockNode::step_after_byte_used_copy(curr);
            VulkanAllocatorBlockNode::step_after_header_inplace(maybe_new_node);
            u32 maybe_new_node_padding = reinterpret_cast<usize>(maybe_new_node) % alignment;

            if (curr->byte_size - curr->byte_used >= sizeof(VulkanAllocatorBlockNode) + maybe_new_node_padding + bytes_need) {
                VulkanAllocatorBlockNode* new_node_header = VulkanAllocatorBlockNode::step_before_header_copy(maybe_new_node);
                new_node_header->padding = maybe_new_node_padding;
                new_node_header->byte_used = bytes_need;

                // byte_size should give proper offset to the next node previously pointed by 'curr' to not break a chain
                new_node_header->byte_size = curr->byte_size - curr->byte_used - sizeof(VulkanAllocatorBlockNode);

                // shrink item 'curr' points to
                curr->byte_size = curr->byte_used;

                _mem_used += maybe_new_node_padding + bytes_need;

                return maybe_new_node;
            }
        }

        // go to the next node
        VulkanAllocatorBlockNode::step_after_byte_size_inplace(curr);
    }

    return nullptr;
}

void VulkanAllocatorBlockList::remove(void* node_to_be_removed) noexcept {
    VulkanAllocatorBlockNode* header = VulkanAllocatorBlockNode::step_before_header_copy(static_cast<VulkanAllocatorBlockNode*>(node_to_be_removed));
    header->padding = 0;
    header->byte_used = 0;
}

// VulkanAllocator
void* vk_alloc_fn(void* user_data, usize alloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_alloc_fn system scope: {}", static_cast<i32>(vk_alloc_scope));

    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);

    // find first block that is not full
    for (auto& block : allocator_state->blocks) {
        if (!block.is_full()) {
            // find free place inside this block
            void* inserted_node = block.insert(static_cast<u32>(alloc_bytes), static_cast<u32>(alignment));
            if (inserted_node) {
                return inserted_node;
            }
        }
    }

    allocator_state->blocks.emplace_back(platform_get_mem_page_size() * 10);
    VulkanAllocatorBlockList& fresh_block = allocator_state->blocks.back();
    void* inserted_node = fresh_block.insert(alloc_bytes, alignment);
    SF_ASSERT_MSG(inserted_node, "Should be non-null pointer");

    return inserted_node;
}

void vk_free_fn(void* user_data, void* node_to_be_removed) {
    LOG_INFO("vk_free call, ptr: {}", node_to_be_removed);

    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);

    for (auto& block : allocator_state->blocks) {
        if (block.is_address_in_range(node_to_be_removed)) {
            block.remove(node_to_be_removed);
        }
    }
}

void* vk_realloc_fn(void* user_data, void* p_original, usize realloc_bytes, usize alignment, VkSystemAllocationScope vk_alloc_scope) {
    LOG_TRACE("vk_realloc_fn call, of size bytes: {}, alignment: {}", realloc_bytes, alignment);
    VulkanAllocator* allocator_state = static_cast<VulkanAllocator*>(user_data);

    allocator_state->callbacks.pfnFree(user_data, p_original);
    return allocator_state->callbacks.pfnAllocation(user_data, realloc_bytes, alignment, vk_alloc_scope);
}

void vk_intern_alloc_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_alloc of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

void vk_intern_free_notification(void* user_data, usize alloc_bytes, VkInternalAllocationType intern_alloc_type, VkSystemAllocationScope vk_alloc_scope) {
    LOG_INFO("vk_intern_free of size bytes: {}, intern type: {}, system type: {}", alloc_bytes, static_cast<i32>(intern_alloc_type), static_cast<i32>(vk_alloc_scope));
}

} // sf
