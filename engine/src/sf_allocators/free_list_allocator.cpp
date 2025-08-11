#include "sf_allocators/free_list_allocator.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

FreeList::FreeList(u32 capacity, bool resizable) noexcept
    : _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    , _head{ reinterpret_cast<FreeListNode*>(_buffer) }
    , _capacity{ capacity }
    , _should_manage_memory{ true }
    , _should_resize{ resizable }
{
    free_all();
}

// UnManaged
FreeList::FreeList(void* data, u32 capacity) noexcept
    : _buffer{ static_cast<u8*>(data) }
    , _head{ reinterpret_cast<FreeListNode*>(_buffer) }
    , _capacity{ capacity }
    , _should_manage_memory{ false }
    , _should_resize{ false }
{
    free_all();
}

FreeList::~FreeList() noexcept
{
    if (_should_manage_memory && _buffer) {
        sf_mem_free(_buffer);
        _buffer = nullptr;
    }
}

void* FreeList::allocate_block(u32 size, u16 alignment) noexcept {
    if (size < MIN_ALLOC_SIZE) {
        size = MIN_ALLOC_SIZE;
    }

    FreeListNode* curr = _head;
    FreeListNode* prev = nullptr;
    u32 padding;
    u32 required_space;

    while (curr) {
        // padding including FreeListAllocHeader
        padding = calc_padding_with_header(curr, alignment, sizeof(FreeListAllocHeader));
        required_space = size + padding;

        if (curr->size >= required_space) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    if (!curr) {
        if (_should_resize) {
            resize(_capacity * 2);
            allocate_block(size, alignment);
        } else {
            return nullptr;
        }
    }

    u32 padding_to_alloc_header = padding - sizeof(FreeListAllocHeader);
    u32 remain_space = curr->size - required_space;

    if (remain_space > MIN_ALLOC_SIZE + sizeof(FreeListNode)) {
        FreeListNode* new_node = ptr_step_bytes_forward(curr, required_space);
        new_node->size = remain_space;
        insert_node(curr, new_node);
    }

    remove_node(prev, curr);

    FreeListAllocHeader* alloc_header = reinterpret_cast<FreeListAllocHeader*>(ptr_step_bytes_forward(curr, padding_to_alloc_header));
    alloc_header->size = size;
    alloc_header->padding = padding;

    return alloc_header + 1;
}

Result<u32> FreeList::allocate_block_and_return_handle(u32 size, u16 alignment) noexcept {
    void* res = allocate_block(size, alignment);
    if (!res) {
        return { .status = false };
    } else {
        return { .data = turn_ptr_into_handle(res, _head), .status = true };
    }
}

bool FreeList::free_block(void* block) noexcept {
    if (is_address_in_range(_head, _capacity, block)) {
        LOG_WARN("Attempt to free block out of FreeList buffer range");
        return false;
    }

    FreeListAllocHeader* header_ptr = static_cast<FreeListAllocHeader*>(ptr_step_bytes_backward(block, sizeof(FreeListAllocHeader)));
    FreeListNode* free_node = static_cast<FreeListNode*>(ptr_step_bytes_backward(block, header_ptr->padding));

    free_node->size = header_ptr->padding + header_ptr->size;
    free_node->next = nullptr;

    FreeListNode* curr = _head;
    FreeListNode* prev = nullptr;

    while (curr) {
        if (curr > free_node) {
            insert_node(prev, free_node);
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    coallescense_nodes(prev, free_node);
    return true;
}

bool FreeList::free_block_with_handle(u32 handle) noexcept {
    void* ptr = turn_handle_into_ptr(handle, _head);
    return free_block(ptr);
}

void FreeList::free_all() noexcept {
    FreeListNode* first_node = reinterpret_cast<FreeListNode*>(_buffer);
    first_node->size = _capacity;
    first_node->next = 0;
    _head = first_node;
}

void FreeList::resize(u32 new_capacity) noexcept {
    u8* new_buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));

    if (new_buffer != _buffer) {
        // make pointers in new_buffer valid
        FreeListNode* old_curr = _head;
        FreeListNode* new_curr = reinterpret_cast<FreeListNode*>(_buffer);

        while (old_curr && new_curr) {
            u32 old_handle = turn_ptr_into_handle(old_curr->next, _buffer);
            FreeListNode* new_ptr = turn_handle_into_ptr<FreeListNode*>(old_handle, new_buffer);

            new_curr->next = new_ptr;

            old_curr = old_curr->next;
            new_curr = new_curr->next;
        }

        sf_mem_free(_buffer);
    }

    _buffer = new_buffer;
    _capacity = new_capacity;
}

void FreeList::insert_node(FreeListNode* prev, FreeListNode* node_to_insert) noexcept {
    if (prev) {
        node_to_insert->next = prev->next;
        prev->next = node_to_insert;
    } else {
        if (_head) {
            node_to_insert->next = _head;
            _head = node_to_insert;
        } else {
            _head = node_to_insert;
        }
    }
}

void FreeList::remove_node(FreeListNode* prev, FreeListNode* node_to_remove) noexcept {
    if (prev) {
        prev->next = node_to_remove->next;
    } else {
        _head = node_to_remove->next;
    }
}

void FreeList::coallescense_nodes(FreeListNode* prev, FreeListNode* free_node) noexcept {
    if (free_node->next && ptr_step_bytes_forward(free_node, free_node->size) == free_node->next) {
        free_node->size += free_node->next->size;
        remove_node(free_node, free_node->next);
    }

    if (prev && ptr_step_bytes_forward(prev, prev->size) == free_node) {
        prev->size += free_node->size;
        remove_node(prev, free_node);
    }
}

u32 FreeList::get_remain_space() noexcept {
    FreeListNode* curr = _head;
    u32 remain_space{0};

    while (curr) {
        remain_space += curr->size;
        curr = curr->next;
    }

    return remain_space;
}

usize FreeList::calc_padding_with_header(void* ptr, u16 alignment, usize header_size) {
	usize p, a, modulo, padding;

	p = reinterpret_cast<usize>(ptr);
	a = alignment;
	modulo = p & (a - 1); // (p % a) as it assumes alignment is a power of two

	padding = 0;

	if (modulo != 0) {
		padding = a - modulo;
	}

	if (padding < header_size) {
		header_size -= padding;

		if ((header_size & (a - 1)) != 0) {
			padding += a * (1 + (header_size / a));
		} else {
			padding += a * (header_size / a);
		}
	}

	return padding;
}

} // sf

