#pragma once

#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_containers/result.hpp"
#include <utility>

namespace sf {
u32 platform_get_mem_page_size();

// ArenaAllocator

// can't shrink or pop from this one, because of padding, and headers are not implemented on purpose
// for allocating global single-item stuff from the start of the program and deallocating all at once at the end
struct ArenaAllocator {
private:
    u32 _capacity;
    u32 _count;
    u8* _buffer;

public:
    ArenaAllocator() noexcept
        : _capacity{ platform_get_mem_page_size() * 10 }
        , _buffer{static_cast<u8*>(sf_mem_alloc(_capacity)) }
        , _count{ 0 }
    {}

    ArenaAllocator(u32 capacity) noexcept
        : _capacity{ capacity }
        , _buffer{static_cast<u8*>(sf_mem_alloc(capacity)) }
        , _count{ 0 }
    {}

    ArenaAllocator(ArenaAllocator&& rhs) noexcept
        : _buffer{ rhs._buffer }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
    {
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    ~ArenaAllocator() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity);
            _buffer = nullptr;
        }
    }

    // returns handle (relative address to start of the buffer)
    template<typename T, typename... Args>
    u32 allocate(Args&&... args) noexcept
    {
        static_assert(is_power_of_two(alignof(T)) && "should be power of 2");

        u32 padding_bytes = sf_calc_padding(_buffer + _count, alignof(T));
        if (_count + padding_bytes + sizeof(T) > _capacity) {
            reallocate(_capacity * 2);
        }

        u32 handle_to_return = _count + padding_bytes;
        _count += padding_bytes + sizeof(T);

        construct_at<T, Args...>(reinterpret_cast<T*>(_buffer + handle_to_return), std::forward<Args>(args)...);

        return handle_to_return;
    }

    u32 allocate(u32 size, u16 alignment) noexcept
    {
        u32 padding_bytes = sf_calc_padding(_buffer + _count, alignment);
        if (_count + padding_bytes + size > _capacity) {
            reallocate(_capacity * 2);
        }

        u32 handle_to_return = _count + padding_bytes;
        _count += padding_bytes + size;

        return handle_to_return;
    }

    void reallocate(u32 new_capacity) {
        u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_capacity));
        sf_mem_copy(new_buffer, _buffer, _count);
        sf_mem_free(_buffer, _capacity);
        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    template<typename T>
    T* get_with_handle(u32 handle) const {
        if (!is_handle_inside_buffer_count(handle)) {
            return nullptr;
        }

        return reinterpret_cast<T*>(_buffer + handle);
    }

    void* get_with_handle(u32 handle) const {
        if (!is_handle_inside_buffer_count(handle)) {
            return nullptr;
        }

        return _buffer + handle;
    }

    bool is_handle_inside_buffer(u32 handle) const {
        return handle < _capacity;
    }

    bool is_handle_inside_buffer_count(u32 handle) const {
        return handle < _count;
    }

    u8* begin() noexcept { return _buffer; }
    u8* data() noexcept { return _buffer; }
    u8* end() noexcept { return _buffer + _count; }
    u32 count() const noexcept { return _count; }
    u32 capacity() const noexcept { return _capacity; }
    // const counterparts
    const u8* begin() const noexcept { return _buffer; }
    const u8* data() const noexcept { return _buffer; }
    const u8* end() const noexcept { return _buffer + _count; }

private:
    template<typename T, typename ...Args>
    void construct_at(T* ptr, Args&&... args) noexcept
    {
        sf_mem_place(ptr, args...);
    }
};

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
    // TODO: should we head?
    FreeListNode* _head;
    u32 _capacity;

public:
    static constexpr usize MIN_ALLOC_SIZE = sizeof(FreeListNode);

    FreeList(void* data, u32 capacity) noexcept;
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
};

} // sf
