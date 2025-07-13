#pragma once

#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include "sf_ds/allocator.hpp"
#include "sf_ds/iterator.hpp"
#include "sf_ds/traits.hpp"
#include <cassert>
#include <concepts>
#include <utility>

namespace sf {

template<typename T, MonoTypeAllocatorTrait<T> A = DefaultArrayAllocator<T>, std::unsigned_integral Utype = u32>
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
    void insert_at(Utype index, Args&&... args) noexcept {
        _allocator.insert_at(index, std::forward<Args>(args)...);
    }

    void reallocate(Utype new_capacity) noexcept {
        _allocator.reallocate(new_capacity);
    }

    void remove_at(Utype index) noexcept {
        _allocator.remove_at(index);
    }

    void remove_unordered_at(Utype index) noexcept {
        _allocator.remove_unordered_at(index);
    }

    void pop() noexcept {
        _allocator.pop();
    }

    void clear() noexcept {
        _allocator.clear();
    }

    void debug_print() const noexcept {
        if constexpr (HasFormatter<T, u8>) {
            for (u32 i{0}; i < _allocator.count(); ++i) {
                LOG_DEBUG("{} ", _allocator.ptr_offset_val(i));
            }
        }
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

    T& operator[](Utype ind) noexcept {
        SF_ASSERT_MSG((ind >= 0 && ind < _allocator.count()), "out of bounds");
        return _allocator.ptr_offset_val(ind);
    }

    const T& operator[](Utype ind) const noexcept {
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

    PtrRandomAccessIterator<T> begin() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.begin());
    }

    PtrRandomAccessIterator<T> end() noexcept {
        return PtrRandomAccessIterator<T>(_allocator.end());
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
        : _allocator{ DefaultArrayAllocator<T>{} }
    {}

    explicit ArrayList(Utype capacity) noexcept
        : _allocator{ DefaultArrayAllocator<T>{ capacity } }
    {}

    ArrayList(std::initializer_list<T> init_list) noexcept
        : _allocator{ DefaultArrayAllocator<T>{ static_cast<Utype>(init_list.size()) } }
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

    ArrayList(std::pair<std::initializer_list<T>, A>&& args) noexcept
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

} // sf
