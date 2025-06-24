#pragma once

// Contiguous doubly linked list
// Fast iteration on "alive" entities
// Remove at random place

#include "sf_core/memory_sf.hpp"
#include "sf_core/types.hpp"
#include_next "sf_core/asserts_sf.hpp"

namespace sf {

template<typename T>
struct LinkedArrayList {
private:
    struct Header {
        u16 forward_count;
        u16 backward_count;
    };

    struct ListNode {
        Header  header;
        T       value;
    };

// data members
private:
    // NOTE: maybe should track how many memory we have at the end
    usize       _capacity;
    usize       _count;
    ListNode*   _buffer;

// helpers
private:
    ListNode* find_free_entry() {
        SF_ASSERT_MSG(_count < _capacity, "called without checking for _count == _capacity");

        ListNode* node_it = _buffer;
        for (; node_it < _buffer + _capacity; ++node_it) {
            if (node_it->header.forward_count == 0) {
                if (_count == 0) {
                    break;
                } else {
                    node_it->header.forward_count = 1;
                    ++node_it;
                    node_it->header.forward_count = 0;
                    node_it->header.backward_count = 1;
                    break;
                }
            } else if (node_it->header.forward_count > 1) {
                ListNode* prev = node_it;
                ++node_it;
                node_it->header.backward_count = 1;
                node_it->header.forward_count = prev->header.forward_count - 1;
                prev->header.forward_count = 1;
            }
        }

        if (node_it < _buffer + _capacity) {
            ++_count;
            return node_it;
        }

        // unreachable
        return nullptr;
    }

    void reallocate() {
        ListNode* new_buffer = sf_mem_alloc(_capacity * sizeof(ListNode) * 2);
        sf_mem_copy(new_buffer, _buffer, _capacity * sizeof(T));
        sf_mem_free(_buffer);
    }

public:
    LinkedArrayList(u64 prealloc_count)
        : _capacity{ prealloc_count }
        , _buffer{ sf_mem_alloc(_capacity * sizeof(ListNode), alignof(ListNode)) }
    {
        _buffer[0].header.forward_count = 0;
        _buffer[0].header.backward_count = 0;
    }

    ListNode* insert(T&& value) {
        if (_count == _capacity) {
            reallocate();
        }

        ListNode* free_node = find_free_entry();
        if (!free_node) {
            reallocate();
        }

        free_node = find_free_entry();
        free_node->value = std::forward<T&&>(value);
        return free_node;
    }

    // TODO: check if already removed
    // either by adding tagged union or zero values in header
    bool remove_at(usize index) {
        if (index < 0 || index >= _capacity) {
            return false;
        }

        ListNode* node = _buffer + index;
        ListNode* prev = node - node->header.backward_count;
        ListNode* next = node + node->header.forward_count;
        prev->header.forward_count += node->header.forward_count;
        next->header.backward_count += node->header.backward_count;

        // NOTE: can we expect this as null value?
        node->header.forward_count = 0;
        node->header.backward_count = 0;
    }

    bool empty() {
        return _buffer[0].header.forward_count == 0 && _buffer[0].header.backward_count == 0;
    }
};

} // sf
