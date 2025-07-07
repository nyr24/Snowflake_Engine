#pragma once

#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/types.hpp"
#include "sf_ds/iterator.hpp"
#include "sf_ds/traits.hpp"
#include <cassert>

namespace sf {

template<typename T, MonoTypeAllocatorTrait<T> A = DefaultArrayAllocator<T>>
struct ArrayList {
private:
    A _allocator;

public:
    using Iterator = PtrRandomAccessIterator<T>;

    ArrayList(std::initializer_list<T> init_list)
        : _allocator{ DefaultArrayAllocator<T>{ init_list.size() } }
    {
        _allocator.allocate(init_list.size());

        u16 i{0};
        auto iter{init_list.begin()};

        for (; iter != init_list.end(); ++iter, ++i) {
            _allocator.construct(
                _allocator.ptr_offset(i),
                *iter
            );
        }
    }

    ArrayList(std::pair<std::initializer_list<T>, A>&& args)
        : _allocator{ std::move(args.second) }
    {
        _allocator.allocate(args.first.size());

        u16 i{0};
        auto iter{args.first.begin()};

        for (; iter != args.first.end(); ++iter, ++i) {
            _allocator.construct(
                _allocator.ptr_offset(i),
                *iter
            );
        }
    }

    ArrayList(A&& _allocator)
        : _allocator{ std::move(_allocator) }
    {}

    ArrayList(const ArrayList<T, A>& rhs)
        : _allocator{ rhs._allocator }
    {}

    ArrayList(ArrayList<T, A>&& rhs) noexcept
        : _allocator{ std::move(rhs._allocator ) }
    {}

    ArrayList<T, A>& operator=(const ArrayList<T, A>& rhs) {
        _allocator = rhs._allocator ;
        return *this;
    }

    ArrayList<T, A>& operator=(ArrayList<T, A>&& rhs) noexcept {
        _allocator = std::move(rhs._allocator );
        return *this;
    }

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        _allocator.allocate_and_construct(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    void append(const T& item) noexcept {
        _allocator.allocate_and_construct(item);
    }

    template<typename ...Args>
    void append(T&& item) noexcept {
        _allocator.allocate_and_construct(item);
    }

    void pop() noexcept {
        _allocator.pop();
    }

    void clear() noexcept {
        _allocator.clear();
    }

    void debug_print() noexcept {
        if constexpr (HasFormatter<T, u8>) {
            for (u32 i{0}; i < _allocator.len(); ++i) {
                LOG_DEBUG("{} ", _allocator.ptr_offset_val(i));
            }
        }
    }

    T& operator[](usize ind) noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < _allocator.len()), "out of bounds");
        return _allocator.ptr_offset_val(ind);
    }

    const T& operator[](usize ind) const noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < _allocator.len()), "out of bounds");
        return _allocator.ptr_offset_val(ind);
    }

    u64 len() noexcept {
        return _allocator.len();
    }

    u64 capacity() noexcept {
        return _allocator.capacity();
    }

    PtrRandomAccessIterator<T> begin() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.begin());
    }

    PtrRandomAccessIterator<T> end() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.end());
    }
};

} // sf
