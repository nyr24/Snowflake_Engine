#pragma once

#include "sf_containers/result.hpp"

namespace sf {

// FreeList allocator
struct FreeListAllocHeader {
    u32 size;
    // padding includes header
    u32 padding;
};

struct FreeListNode {
    FreeListNode* next;
    u32 size;
};

struct FreeList {
private:
    u8* _buffer;
    FreeListNode* _head;
    u32 _capacity;
    bool _should_manage_memory;
    bool _should_resize;

public:
    static constexpr usize MIN_ALLOC_SIZE = sizeof(FreeListNode);

    FreeList(u32 capacity, bool resizable) noexcept;
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
    usize calc_padding_with_header(void* ptr, u16 alignment, usize header_size);
    void resize(u32 new_capacity) noexcept;
    constexpr u8* begin() noexcept { return _buffer; }
    constexpr u32 total_size() noexcept { return _capacity; };
};

} // sf
