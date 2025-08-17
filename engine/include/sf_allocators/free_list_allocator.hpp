#pragma once

#include "sf_containers/result.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

struct FreeListAllocHeader {
    u32 size;
    // padding includes header
    u32 padding;
};

struct FreeListNode {
    FreeListNode* next;
    u32 size;
};

struct FreeListConfig {
    bool should_manage_memory;
    bool resizable;
};

template<FreeListConfig Config>
struct FreeList {
private:
    u8* _buffer;
    FreeListNode* _head;
    u32 _capacity;

public:
    static constexpr usize MIN_ALLOC_SIZE = sizeof(FreeListNode);

    FreeList(u32 capacity) noexcept;
    FreeList(void* data, u32 capacity) noexcept;
    ~FreeList() noexcept;
    void* allocate_block(u32 size, u16 alignment) noexcept;
    Result<u32> allocate_block_and_return_handle(u32 size, u16 alignment) noexcept;
    bool free_block(void* ptr) noexcept;
    bool free_block_with_handle(u32 handle) noexcept;
    void free_all() noexcept;
    void insert_node(FreeListNode* prev, FreeListNode* node_to_insert) noexcept;
    void coallescense_nodes(FreeListNode* prev, FreeListNode* free_node) noexcept;
    void remove_node(FreeListNode* prev, FreeListNode* node_to_remove) noexcept;
    u32 get_remain_space() noexcept;
    void resize(u32 new_capacity) noexcept;
    constexpr u8* begin() noexcept { return _buffer; }
    constexpr u32 total_size() noexcept { return _capacity; };
};

template<FreeListConfig Config>
FreeList<Config>
::FreeList(u32 capacity) noexcept
    : _buffer{ static_cast<u8*>(sf_mem_alloc(capacity)) }
    , _head{ reinterpret_cast<FreeListNode*>(_buffer) }
    , _capacity{ capacity }
{
    static_assert(Config.should_manage_memory && "Wrong constructor call or template parameter, this one manages memory yourself");
    free_all();
}

// UnManaged
template<FreeListConfig Config>
FreeList<Config>
::FreeList(void* data, u32 capacity) noexcept
    : _buffer{ static_cast<u8*>(data) }
    , _head{ reinterpret_cast<FreeListNode*>(_buffer) }
    , _capacity{ capacity }
{
    static_assert(!Config.should_manage_memory && "Wrong constructor call or template parameter, this one don't manages memory yourself");
    free_all();
}

template<FreeListConfig Config>
FreeList<Config>
::~FreeList() noexcept
{
    if constexpr (Config.should_manage_memory) {
        if (_buffer) {
            sf_mem_free(_buffer);
            _buffer = nullptr;
        }
    }
}

template<FreeListConfig Config>
void* FreeList<Config>::allocate_block(u32 size, u16 alignment) noexcept {
    if (size < MIN_ALLOC_SIZE) {
        size = MIN_ALLOC_SIZE;
    }
    if (alignment < sizeof(usize)) {
        alignment = sizeof(usize);
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
        if constexpr (Config.resizable) {
            resize(_capacity * 2);
            return allocate_block(size, alignment);
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

template<FreeListConfig Config>
Result<u32> FreeList<Config>::allocate_block_and_return_handle(u32 size, u16 alignment) noexcept {
    void* res = allocate_block(size, alignment);
    if (!res) {
        return {ResultError::VALUE};
    } else {
        return {turn_ptr_into_handle(res, _buffer)};
    }
}

template<FreeListConfig Config>
bool FreeList<Config>::free_block(void* block) noexcept {
    if (!is_address_in_range(_buffer, _capacity, block)) {
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

template<FreeListConfig Config>
bool FreeList<Config>::free_block_with_handle(u32 handle) noexcept {
    void* ptr = turn_handle_into_ptr(handle, _buffer);
    return free_block(ptr);
}

template<FreeListConfig Config>
void FreeList<Config>::free_all() noexcept {
    FreeListNode* first_node = reinterpret_cast<FreeListNode*>(_buffer);
    first_node->size = _capacity;
    first_node->next = 0;
    _head = first_node;
}

template<FreeListConfig Config>
void FreeList<Config>::resize(u32 new_capacity) noexcept {
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

    if (new_buffer != _buffer) {
        sf_mem_free(_buffer);
        _buffer = new_buffer;
    }

    _capacity = new_capacity;
}

template<FreeListConfig Config>
void FreeList<Config>::insert_node(FreeListNode* prev, FreeListNode* node_to_insert) noexcept {
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

template<FreeListConfig Config>
void FreeList<Config>::remove_node(FreeListNode* prev, FreeListNode* node_to_remove) noexcept {
    if (prev) {
        prev->next = node_to_remove->next;
    } else {
        _head = node_to_remove->next;
    }
}

template<FreeListConfig Config>
void FreeList<Config>::coallescense_nodes(FreeListNode* prev, FreeListNode* free_node) noexcept {
    if (free_node->next && ptr_step_bytes_forward(free_node, free_node->size) == free_node->next) {
        free_node->size += free_node->next->size;
        remove_node(free_node, free_node->next);
    }

    if (prev && ptr_step_bytes_forward(prev, prev->size) == free_node) {
        prev->size += free_node->size;
        remove_node(prev, free_node);
    }
}

template<FreeListConfig Config>
u32 FreeList<Config>::get_remain_space() noexcept {
    FreeListNode* curr = _head;
    u32 remain_space{0};

    while (curr) {
        remain_space += curr->size;
        curr = curr->next;
    }

    return remain_space;
}

} // sf
