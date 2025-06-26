#pragma once

// Contiguous doubly linked list
// Fast iteration on "alive" entities
// Fast remove at random index

#include "sf_core/memory_sf.hpp"
#include "sf_core/types.hpp"
#include "sf_core/asserts_sf.hpp"
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sf {

template<typename T>
struct LinkedArrayList {
private:
    struct Header {
        u16 forward_count;
        u16 backward_count;
    };

    struct ListNode {
        T       value;
        Header  header;
    };

    struct LinkedArrayListIterator {
    protected:
        ListNode* ptr;
    public:
        using ValueType = ListNode;
        using Ptr = ValueType*;
        using LvalueRef = ValueType&;
        using RvalueRef = ValueType&&;
        using ConstRef = const ValueType&;
        using Diff = std::ptrdiff_t;
        using Strategy = std::random_access_iterator_tag;

        LinkedArrayListIterator() = default;
        ~LinkedArrayListIterator() = default;
        LinkedArrayListIterator(const LinkedArrayListIterator& rhs) = default;
        LinkedArrayListIterator& operator=(const LinkedArrayListIterator& rhs) = default;

        LinkedArrayListIterator(ListNode* ptr)
            : ptr{ ptr }
        {}

        ListNode&& operator*() const {
            return *ptr;
        }

        ListNode& operator*() {
            return *ptr;
        }

        ListNode* operator->() {
            return ptr;
        }

        // it++
        LinkedArrayListIterator operator++(i32 offset) {
            LinkedArrayListIterator tmp{ *this };
            ptr += ptr->header.forward_count;
            return tmp;
        }

        // ++it
        LinkedArrayListIterator& operator++() {
            ptr += ptr->header.forward_count;
            return *this;
        }

        // it--
        LinkedArrayListIterator operator--(i32 offset) {
            LinkedArrayListIterator tmp{ *this };
            ptr -= ptr->header.backward_count;
            return tmp;
        }

        // --it
        LinkedArrayListIterator& operator--() {
            ptr -= ptr->header.backward_count;
            return *this;
        }

        T& operator[](usize index) {
            for (usize i{0}; i < index; ++i) {
                ptr += ptr->header.forward_count;
            }
            return *ptr;
        }

        LinkedArrayListIterator& operator+=(i32 val) {
            for (usize i{0}; i < val; ++i) {
                ptr += ptr->header.forward_count;
            }
            return *this;
        }

        LinkedArrayListIterator& operator-=(i32 val) {
            for (usize i{0}; i < val; ++i) {
                ptr -= ptr->header.backward_count;
            }
            return *this;
        }

        friend bool operator==(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr == second.ptr;
        }

        friend bool operator!=(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr != second.ptr;
        }

        friend LinkedArrayListIterator operator-(const LinkedArrayListIterator& first, const LinkedArrayListIterator & second) {
            return LinkedArrayListIterator{ first.ptr - second.ptr };
        }

        friend LinkedArrayListIterator operator+(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return LinkedArrayListIterator{ first.ptr + second.ptr };
        }

        friend bool operator>(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr > second.ptr;
        }

        friend bool operator<(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr < second.ptr;
        }

        friend bool operator>=(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr >= second.ptr;
        }

        friend bool operator<=(const LinkedArrayListIterator& first, const LinkedArrayListIterator& second) {
            return first.ptr <= second.ptr;
        }
    };

// data members
private:
    usize       _capacity;
    usize       _count;
    ListNode*   _buffer;
    u32         _head_offset;
    u32         _tail_offset;

public:
    using ListNodeType = ListNode;

    LinkedArrayList(u64 prealloc_count) noexcept
        : _capacity{ prealloc_count }
        , _buffer{ static_cast<ListNode*>(sf_mem_alloc(_capacity * sizeof(ListNode), alignof(ListNode))) }
        , _head_offset{0}
        , _tail_offset{0}
    {
        zero_node(_buffer);
    }

    LinkedArrayList(std::pair<std::initializer_list<T>, u64>&& args) noexcept
        : _capacity{ args.second }
        , _buffer{ static_cast<ListNode*>(sf_mem_alloc(_capacity * sizeof(ListNode), alignof(ListNode))) }
        , _head_offset{0}
        , _tail_offset{0}
    {
        for (auto it = args.first.begin(); it != args.first.end(); ++it) {
            insert(*it);
        }
    }

    ~LinkedArrayList() noexcept
    {
        if (_buffer) {
            sf_mem_free(_buffer, _capacity * sizeof(ListNode), alignof(ListNode));
            _buffer = nullptr;
        }
    }

    template<typename U>
    requires std::same_as<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>
    void insert(U&& value) noexcept {
        if (_count == _capacity) {
            reallocate();
        }

        if (_count == 0) {
            insert_first(value);
        } else if (_tail_offset < (_capacity - 1)) {
            insert_end(std::forward<U>(value));
        } else if (_head_offset > 0) {
            insert_begin(std::forward<U>(value));
        } else {
            insert_search(std::forward<U>(value));
        }
    }

    void remove_at(usize index) {
        if (_count == 0 || index < _head_offset || index > _tail_offset) {
            return;
        }

        ListNode* node = _buffer + index;
        if (is_node_empty(node)) {
            return;
        }

        bool is_node_head = is_head(node);
        bool is_node_tail = !is_node_head && is_tail(node);

        if (is_node_head) {
            ListNode* next = node + node->header.forward_count;
            next->header.backward_count = 1;
            _head_offset += node->header.forward_count;
        } else if (is_node_tail) {
            ListNode* prev = node - node->header.backward_count;
            // needs to be positive for iteration
            prev->header.forward_count = 1;
            _tail_offset -= node->header.backward_count;
        } else {
            ListNode* prev = node - node->header.backward_count;
            ListNode* next = node + node->header.forward_count;
            prev->header.forward_count += node->header.forward_count;
            next->header.backward_count += node->header.backward_count;
        }

        zero_node(node);
        --_count;
    }

    bool empty() {
        return (_buffer + _head_offset).header.forward_count == 0 && (_buffer + _tail_offset).header.backward_count == 0;
    }

    usize count() {
        return _count;
    }

    usize capacity() {
        return _capacity;
    }

    LinkedArrayListIterator begin() {
        return LinkedArrayListIterator(_buffer + _head_offset);
    }

    LinkedArrayListIterator end() {
        return LinkedArrayListIterator(_buffer + _tail_offset + 1);
    }

private:
    template<typename U>
    requires std::same_as<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>
    void insert_first(U&& value) noexcept {
        SF_ASSERT_MSG(_count == 0, "should not have elements");
        _buffer->header.forward_count = 1;
        _buffer->header.backward_count = 1;
        _buffer->value = std::forward<U>(value);
        ++_count;
    }

    template<typename U>
    requires std::same_as<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>
    void insert_end(U&& value) noexcept {
        SF_ASSERT_MSG(_tail_offset < (_capacity - 1), "should have place at tail");

        ListNode* tail = _buffer + _tail_offset;
        tail->header.forward_count = 1;

        ++tail;
        tail->header.backward_count = 1;
        tail->header.forward_count = 1;
        tail->value = std::forward<U>(value);

        ++_count;
        ++_tail_offset;
    }

    template<typename U>
    requires std::same_as<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>
    void insert_begin(U&& value) noexcept {
        SF_ASSERT_MSG(_head_offset > 0, "should have place at head");

        ListNode* head = _buffer + _head_offset;
        head->header.backward_count = 1;
        --head;
        head->header.forward_count = 1;
        // needs to be positive for backwards iteration
        head->header.backward_count = 1;
        head->value = std::forward<U>(value);

        ++_count;
        --_head_offset;
    }

    template<typename U>
    requires std::same_as<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>
    void insert_search(U&& value) noexcept {
        SF_ASSERT_MSG(_tail_offset >= _capacity - 1, "use insert_end instead");
        SF_ASSERT_MSG(_head_offset == 0, "use insert_begin instead");
        SF_ASSERT_MSG(_count < _capacity, "should have place at buffer somewhere");

        ListNode* free_node = find_free_entry();
        free_node->value = std::forward<U>(value);
        ++_count;
    }

    ListNode* find_free_entry() {
        SF_ASSERT_MSG(_count < _capacity, "called without checking for _count == _capacity");

        ListNode* node_it = _buffer;
        for (; node_it < _buffer + _capacity; ++node_it) {
            if (node_it->header.forward_count > 1) {
                ListNode* prev = node_it;
                ++node_it;
                node_it->header.backward_count = 1;
                node_it->header.forward_count = prev->header.forward_count - 1;
                prev->header.forward_count = 1;
                break;
            }
        }

        return node_it;
    }

    void reallocate() {
        usize new_capacity = _capacity * 2;
        ListNode* new_buffer = static_cast<ListNode*>(sf_mem_alloc(new_capacity * sizeof(ListNode), alignof(ListNode)));
        sf_mem_copy(new_buffer, _buffer, _capacity * sizeof(ListNode));
        sf_mem_free(_buffer, _capacity * sizeof(ListNode), alignof(ListNode));

        _buffer = new_buffer;
        _capacity = new_capacity;
    }

    bool is_node_empty(ListNode* node) {
        return node->header.backward_count == 0 && node->header.forward_count == 0;
    }

    bool is_head(ListNode* node) {
        return node == _buffer + _head_offset;
    }

    bool is_tail(ListNode* node) {
        return node == _buffer + _tail_offset;
    }

    void zero_node(ListNode* node) {
        node->header.backward_count = 0;
        node->header.forward_count = 0;
    }
}; // LinkedArrayList

} // sf
