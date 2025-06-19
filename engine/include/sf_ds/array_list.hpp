#pragma once

#include "sf_core/memory_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/types.hpp"
#include <cassert>
#include <initializer_list>

namespace sf {

template<typename T, Allocator<T> A>
struct ArrayList {
public:
    ArrayList(std::pair<A, std::initializer_list<T>>&& args);
    ArrayList(A&& allocator);
    // ArrayList(const ArrayList<T, A>& rhs);
    // ArrayList(ArrayList<T, A>&& rhs) noexcept;
    // ArrayList<T, A>& operator=(const ArrayList<T, A>& rhs);
    // ArrayList<T, A>& operator=(ArrayList<T, A>&& rhs) noexcept;
    // ~ArrayList();

    template<typename ...Args>
    void append_emplace(Args&&... args) noexcept;
    template<typename ...Args>
    void append(const T& item) noexcept;
    template<typename ...Args>
    void append(T&& item) noexcept;
    void pop() noexcept;
    void debug_print() noexcept;

    u64 len() noexcept;
    u64 capacity() noexcept;

    T& operator[](usize ind) noexcept;
    const T& operator[](usize ind) const noexcept;

private:
    A allocator;
};

// implementation

template<typename T, Allocator<T> A>
ArrayList<T, A>::ArrayList(std::pair<A, std::initializer_list<T>>&& args)
    : allocator{ std::move(args.first) }
{
    allocator.allocate(args.second.size());
    std::copy(args.second.begin(), args.second.end(), allocator.buffer);
    allocator.len = args.second.size();
}

template<typename T, Allocator<T> A>
ArrayList<T, A>::ArrayList(A&& allocator)
    : allocator{ std::move(allocator) }
{}

template<typename T, Allocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append_emplace(Args&&... args) noexcept {
    T* place_ptr = allocator.allocate(1);
    allocator.construct(place_ptr, std::forward<Args>(args)...);
}

template<typename T, Allocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append(const T& item) noexcept {
    T* place_ptr = allocator.allocate(1);
    allocator.construct(place_ptr, std::forward<Args>(item)...);
}

template<typename T, Allocator<T> A>
template<typename ...Args>
void ArrayList<T, A>::append(T&& item) noexcept {
    T* place_ptr = allocator.allocate(1);
    allocator.construct(place_ptr, std::forward<Args>(item)...);
}

template<typename T, Allocator<T> A>
void ArrayList<T, A>::pop() noexcept {
    if (allocator.len > 0) {
        allocator.len--;
    }
}

template<typename T, Allocator<T> A>
void ArrayList<T, A>::debug_print() noexcept {
    for (u16 i{0}; i < allocator.len; ++i) {
        LOG_DEBUG("{} ", allocator.buffer[i]);
    }
    LOG_DEBUG("\n");
}

template<typename T, Allocator<T> A>
T& ArrayList<T, A>::operator[](usize ind) noexcept {
    assert(ind > 0 && ind < allocator.len && "out of bounds");
    return reinterpret_cast<T*>(allocator.buffer)[ind];
}

template<typename T, Allocator<T> A>
const T& ArrayList<T, A>::operator[](usize ind) const noexcept {
    assert(ind > 0 && ind < allocator.len && "out of bounds");
    return reinterpret_cast<T*>(allocator.buffer)[ind];
}

template<typename T, Allocator<T> A>
u64 ArrayList<T, A>::len() noexcept {
    return allocator.len;
}

template<typename T, Allocator<T> A>
u64 ArrayList<T, A>::capacity() noexcept {
    return allocator.capacity;
}

} // sf
