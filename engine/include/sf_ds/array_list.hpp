#pragma once

#include "sf_core/defines.hpp"
#include "sf_ds/allocator.hpp"
#include "sf_ds/iterator.hpp"
#include "sf_ds/traits.hpp"
#include <cassert>
#include <utility>

namespace sf {

template<typename T, MonoTypeAllocatorTrait<T> A = DefaultArrayAllocator<T>>
struct ArrayList {
private:
    A _allocator;

public:
    using Iterator = PtrRandomAccessIterator<T>;

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept {
        _allocator.allocate_and_construct(std::forward<Args>(args)...);
    }

    void append(const T& item) noexcept {
        _allocator.allocate_and_construct(item);
    }

    void append(T&& item) noexcept {
        _allocator.allocate_and_construct(std::move(item));
    }

    template<typename ...Args>
    void insert_at(u32 index, Args&&... args) noexcept {
        _allocator.insert_at(index, std::forward<Args>(args)...);
    }

    void reallocate(u32 new_capacity) noexcept {
        _allocator.reallocate(new_capacity);
    }

    void remove_at(u32 index) noexcept {
        _allocator.remove_at(index);
    }

    void remove_unordered_at(u32 index) noexcept {
        _allocator.remove_unordered_at(index);
    }

    void pop() noexcept {
        _allocator.pop();
    }

    void clear() noexcept {
        _allocator.clear();
    }

    static u64 hash(const sf::ArrayList<T, A>& key) noexcept {
        constexpr u64 PRIME = 1099511628211u;
        constexpr u64 OFFSET_BASIS = 14695981039346656037u;

        u64 hash = OFFSET_BASIS;

        for (u32 i{0}; i < key.count() * sizeof(T); ++i) {
            hash ^= reinterpret_cast<u8*>(key.data())[i];
            hash *= PRIME;
        }

        return hash;
    }

    T& operator[](u32 ind) noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < _allocator.count()), "out of bounds");
        return _allocator.ptr_offset_val(ind);
    }

    const T& operator[](u32 ind) const noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < _allocator.count()), "out of bounds");
        return _allocator.ptr_offset_val(ind);
    }

    friend bool operator==(const ArrayList<T, A>& first, const ArrayList<T, A>& second) noexcept {
        return first._allocator == second._allocator;
    }

    u64 count() const noexcept {
        return _allocator.count();
    }

    u64 capacity() const noexcept {
        return _allocator.capacity();
    }

    u64 capacity_remain() const noexcept {
        return _allocator.capacity_remain();
    }

    PtrRandomAccessIterator<T> begin() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.begin());
    }

    PtrRandomAccessIterator<T> end() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.end());
    }

    T* last() noexcept {
        return _allocator.end();
    }

    T* last_before_one() noexcept {
        return _allocator.end_before_one();
    }

    T* data() noexcept {
        return _allocator.begin();
    }

    // const versions
    PtrRandomAccessIterator<const T> begin() const noexcept {
        return PtrRandomAccessIterator<const T>(_allocator.begin());
    }

    PtrRandomAccessIterator<const T> end() const noexcept {
        return PtrRandomAccessIterator<const T>(_allocator.end());
    }

    const T* data() const noexcept {
        return _allocator.begin();
    }

    // constructors and assignments
    ArrayList() noexcept
        : _allocator{ A{} }
    {}

    explicit ArrayList(u32 capacity) noexcept
        : _allocator{ A(capacity) }
    {}

    explicit ArrayList(u32 capacity, u32 count) noexcept
        : _allocator{ A(capacity, count) }
    {}

    ArrayList(std::initializer_list<T> init_list) noexcept
        : _allocator{ A(init_list.size()) }
    {
        _allocator.allocate(init_list.size());

        u16 i{0};
        auto iter{init_list.begin()};

        for (; iter != init_list.end(); ++iter, ++i) {
            _allocator.construct_at(
                _allocator.ptr_offset(i),
                *iter
            );
        }
    }

    ArrayList(std::pair<std::initializer_list<T>, A>&& args) noexcept
        : _allocator{ std::move(args.second) }
    {
        _allocator.allocate(args.first.size());

        u16 i{0};
        auto iter{args.first.begin()};

        for (; iter != args.first.end(); ++iter, ++i) {
            _allocator.construct_at(
                _allocator.ptr_offset(i),
                *iter
            );
        }
    }

    ArrayList(A&& _allocator) noexcept
        : _allocator{ std::move(_allocator) }
    {}

    ArrayList(const ArrayList<T, A>& rhs) noexcept
        : _allocator{ rhs._allocator }
    {}

    ArrayList(ArrayList<T, A>&& rhs) noexcept
        : _allocator{ std::move(rhs._allocator ) }
    {}

    ArrayList<T, A>& operator=(const ArrayList<T, A>& rhs) noexcept {
        _allocator = rhs._allocator ;
        return *this;
    }

    ArrayList<T, A>& operator=(ArrayList<T, A>&& rhs) noexcept {
        _allocator = std::move(rhs._allocator );
        return *this;
    }
};

template<typename T, u32 Capacity>
using FixedArrayList = ArrayList<T, FixedBufferAllocator<T, Capacity>>;

} // sf
