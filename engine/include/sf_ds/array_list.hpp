#pragma once

#include "sf_core/memory_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_ds/iterator.hpp"
#include <cassert>
#include <initializer_list>

namespace sf {

template<typename T, MonoTypeAllocator<T> A>
struct ArrayList {
private:
    A allocator;

public:
    using Iterator = RandomAccessIterator<T>;

    ArrayList(std::pair<std::initializer_list<T>, A>&& args)
        : allocator{ std::move(args.second) }
    {
        allocator.allocate(args.first.size());

        u16 i{0};
        auto iter{args.first.begin()};

        for (; iter != args.first.end(); ++iter, ++i) {
            allocator.construct(
                allocator.ptr_offset(i),
                *iter
            );
        }
    }

    ArrayList(A&& allocator)
        : allocator{ std::move(allocator) }
    {}

    ArrayList(const ArrayList<T, A>& rhs)
        : allocator{ rhs.allocator }
    {}

    ArrayList(ArrayList<T, A>&& rhs) noexcept
        : allocator{ std::move(rhs.allocator) }
    {}

    ArrayList<T, A>& operator=(const ArrayList<T, A>& rhs) {
        allocator = rhs.allocator;
        return *this;
    }

    ArrayList<T, A>& operator=(ArrayList<T, A>&& rhs) noexcept {
        allocator = std::move(rhs.allocator);
        return *this;
    }

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        allocator.allocate_and_construct(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    void append(const T& item) noexcept {
        allocator.allocate_and_construct(item);
    }

    template<typename ...Args>
    void append(T&& item) noexcept {
        allocator.allocate_and_construct(item);
    }

    void pop() noexcept {
        if (allocator.len() > 0) {
            allocator.pop();
        }
    }

    void debug_print() noexcept {
        if constexpr (HasFormatter<T, u8>) {
            for (u32 i{0}; i < allocator.len(); ++i) {
                LOG_DEBUG("{} ", allocator.ptr_offset_val(i));
            }
        }
    }

    T& operator[](usize ind) noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < allocator.len()), "out of bounds");
        return allocator.ptr_offset_val(ind);
    }

    const T& operator[](usize ind) const noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < allocator.len()), "out of bounds");
        return allocator.ptr_offset_val(ind);
    }

    u64 len() noexcept {
        return allocator.len();
    }

    u64 capacity() noexcept {
        return allocator.capacity();
    }

    RandomAccessIterator<T> begin() noexcept {
        return RandomAccessIterator<T>(allocator.begin());
    }

    RandomAccessIterator<T> end() noexcept {
        return RandomAccessIterator<T>(allocator.end());
    }
};

} // sf
