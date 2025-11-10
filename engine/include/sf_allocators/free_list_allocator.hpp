#pragma once

#include "sf_containers/traits.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

struct FreeListAllocHeader {
    usize size;
    // padding includes header
    usize padding;
};

struct FreeListNode {
    FreeListNode* next;
    usize size;
};

template<bool RESIZABLE = true>
struct FreeList {
private:
    u8*           _buffer;
    FreeListNode* _head;
    usize         _capacity;

public:
    static constexpr usize MIN_ALLOC_SIZE = sizeof(FreeListNode);

    FreeList(usize capacity) noexcept;
    FreeList(void* data, usize capacity) noexcept;
    ~FreeList() noexcept;
    void* allocate(usize size, u16 alignment) noexcept;
    usize allocate_handle(usize size, u16 alignment) noexcept;
    void* handle_to_ptr(usize handle) const noexcept;
    usize ptr_to_handle(void* ptr) const noexcept;
    ReallocReturn reallocate(void* addr, usize new_size, u16 alignment) noexcept;
    ReallocReturnHandle reallocate_handle(usize handle, usize new_size, u16 alignment) noexcept;
    void free(void* ptr) noexcept;
    void free_handle(usize handle) noexcept;
    void clear() noexcept;
    void insert_node(FreeListNode* prev, FreeListNode* node_to_insert) noexcept;
    void coallescense_nodes(FreeListNode* prev, FreeListNode* free_node) noexcept;
    void remove_node(FreeListNode* prev, FreeListNode* node_to_remove) noexcept;
    usize get_remain_space() noexcept;
    void resize(usize new_capacity) noexcept;
    constexpr u8* begin() noexcept { return _buffer; }
    constexpr usize total_size() noexcept { return _capacity; };
};

template<bool RESIZABLE>
FreeList<RESIZABLE>
::FreeList(usize capacity) noexcept
    : _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    , _head{ reinterpret_cast<FreeListNode*>(_buffer) }
    , _capacity{ capacity }
{
    clear();
}

template<bool RESIZABLE>
FreeList<RESIZABLE>
::~FreeList() noexcept
{
    if (_buffer) {
        sf_mem_free(_buffer);
        _buffer = nullptr;
    }
}

template<bool RESIZABLE>
void* FreeList<RESIZABLE>::allocate(usize size, u16 alignment) noexcept {
    if (size < MIN_ALLOC_SIZE) {
        size = MIN_ALLOC_SIZE;
    }
    if (alignment < sizeof(usize)) {
        alignment = sizeof(usize);
    }

    FreeListNode* curr = _head;
    FreeListNode* prev = nullptr;
    usize padding;
    usize required_space;

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
        if constexpr (RESIZABLE) {
            resize(_capacity * 2);
            return allocate(size, alignment);
        } else {
            return nullptr;
        }
    }

    usize padding_to_alloc_header = padding - sizeof(FreeListAllocHeader);
    usize remain_space = curr->size - required_space;

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

template<bool RESIZABLE>
usize FreeList<RESIZABLE>::allocate_handle(usize size, u16 alignment) noexcept {
    void* res = allocate(size, alignment);
    return turn_ptr_into_handle(res, _buffer);
}

template<bool RESIZABLE>
ReallocReturn FreeList<RESIZABLE>::reallocate(void* addr, usize new_size, u16 alignment) noexcept {
    if (!is_address_in_range(_buffer, _capacity, addr)) {
        return {nullptr, false};
    }

    void* res = allocate(new_size, alignment);
    FreeListAllocHeader* header_ptr = static_cast<FreeListAllocHeader*>(ptr_step_bytes_backward(addr, sizeof(FreeListAllocHeader)));
    // copy old memory to the new chunk
    sf_mem_copy(res, addr, header_ptr->size);
    free(addr);
    return {res, false};
}

template<bool RESIZABLE>
ReallocReturnHandle FreeList<RESIZABLE>::reallocate_handle(usize handle, usize new_size, u16 alignment) noexcept {
    if (!is_handle_in_range(_buffer, _capacity, handle)) {
        return {INVALID_ALLOC_HANDLE, false};
    }

    ReallocReturn realloc_res = reallocate(turn_handle_into_ptr(handle, _buffer), new_size, alignment);
    return {turn_ptr_into_handle(realloc_res.ptr, _buffer), realloc_res.should_mem_copy};
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::free(void* block) noexcept {
    if (!is_address_in_range(_buffer, _capacity, block)) {
        LOG_WARN("Attempt to free block out of FreeList buffer range");
        return;
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
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::free_handle(usize handle) noexcept {
    void* ptr = turn_handle_into_ptr(handle, _buffer);
    free(ptr);
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::clear() noexcept {
    FreeListNode* first_node = reinterpret_cast<FreeListNode*>(_buffer);
    first_node->size = _capacity;
    first_node->next = 0;
    _head = first_node;
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::resize(usize new_capacity) noexcept {
    u8* new_buffer = static_cast<u8*>(sf_mem_realloc(_buffer, new_capacity));

    // append node at the back
    FreeListNode* free_node = reinterpret_cast<FreeListNode*>(new_buffer + _capacity);
    free_node->size = new_capacity - _capacity;
    free_node->next = nullptr;

    FreeListNode* last_node;
    FreeListNode* prev = nullptr;

    if (new_buffer != _buffer) {
        FreeListNode* old_head = _head;

        if (old_head) {
            _head = rebase_ptr<FreeListNode*>(old_head, _buffer, new_buffer);
        }

        // 1) revalidate pointers
        // 2) find last available node, which is closer to the back
        FreeListNode* old_curr = old_head;
        FreeListNode* new_curr = _head;

        while (old_curr && new_curr) {
            if (old_curr->next) {
                FreeListNode* new_next_ptr = rebase_ptr<FreeListNode*>(old_curr->next, _buffer, new_buffer);
                new_curr->next = new_next_ptr;
            } else {
                // last available node
                last_node = new_curr;
                break;
            }

            old_curr = old_curr->next;
            prev = new_curr;
            new_curr = new_curr->next;
        }
    } else {
        last_node = _head;
        while (last_node && last_node->next) {
            prev = last_node;
            last_node = last_node->next;
        }
    }

    insert_node(last_node, free_node);
    coallescense_nodes(prev, last_node);

    _buffer = new_buffer;
    _capacity = new_capacity;
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::insert_node(FreeListNode* prev, FreeListNode* node_to_insert) noexcept {
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

template<bool RESIZABLE>
void FreeList<RESIZABLE>::remove_node(FreeListNode* prev, FreeListNode* node_to_remove) noexcept {
    if (prev) {
        prev->next = node_to_remove->next;
    } else {
        _head = node_to_remove->next;
    }
}

template<bool RESIZABLE>
void FreeList<RESIZABLE>::coallescense_nodes(FreeListNode* prev, FreeListNode* free_node) noexcept {
    if (free_node && free_node->next && ptr_step_bytes_forward(free_node, free_node->size) == free_node->next) {
        free_node->size += free_node->next->size;
        remove_node(free_node, free_node->next);
    }

    if (prev && ptr_step_bytes_forward(prev, prev->size) == free_node) {
        prev->size += free_node->size;
        remove_node(prev, free_node);
    }
}

template<bool RESIZABLE>
usize FreeList<RESIZABLE>::get_remain_space() noexcept {
    FreeListNode* curr = _head;
    usize remain_space{0};

    while (curr) {
        remain_space += curr->size;
        curr = curr->next;
    }

    return remain_space;
}

template<bool RESIZABLE>
void* FreeList<RESIZABLE>::handle_to_ptr(usize handle) const noexcept {
#ifdef SF_DEBUG
    if (!is_handle_in_range(_buffer, _capacity, handle) || handle == INVALID_ALLOC_HANDLE) {
        return nullptr;
    }
#endif
    
    return _buffer + handle;
}

template<bool RESIZABLE>
usize FreeList<RESIZABLE>::ptr_to_handle(void* ptr) const noexcept {
#ifdef SF_DEBUG
    if (!is_address_in_range(_buffer, _capacity, ptr) || ptr == nullptr) {
        return INVALID_ALLOC_HANDLE;
    }
#endif

    return turn_ptr_into_handle(ptr, _buffer);
}

} // sf
