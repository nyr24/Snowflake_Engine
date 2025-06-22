#pragma once
#include "sf_core/types.hpp"
#include <cstddef>

namespace sf {

// RandomAccessIterator
template<typename T>
struct RandomAccessIterator {
public:
    using ValueType = T;
    using Ptr = T*;
    using LvalueRef = T&;
    using RvalueRef = T&&;
    using ConstRef = const T&;
    using Diff = std::ptrdiff_t;

    RandomAccessIterator(T* ptr);
    RandomAccessIterator() = default;
    ~RandomAccessIterator() = default;
    RandomAccessIterator(const RandomAccessIterator<T>& rhs) = default;
    RandomAccessIterator<T>& operator=(const RandomAccessIterator<T>& rhs) = default;
    T&& operator*() const;
    T& operator*();
    T* operator->();
    RandomAccessIterator<T> operator++(i32 offset);
    RandomAccessIterator<T>& operator++();
    RandomAccessIterator<T> operator--(i32 offset);
    RandomAccessIterator<T>& operator--();
    RandomAccessIterator<T>& operator+=(i32 val);
    RandomAccessIterator<T>& operator-=(i32 val);
    T& operator[](usize index);
    const T& operator[](usize index) const;

    friend bool operator==(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr == second.ptr;
    }

    friend bool operator!=(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr != second.ptr;
    }

    friend bool operator>(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr > second.ptr;
    }

    friend bool operator<(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr < second.ptr;
    }

    friend bool operator>=(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr >= second.ptr;
    }

    friend bool operator<=(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return first.ptr <= second.ptr;
    }

    friend RandomAccessIterator<T> operator+(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return RandomAccessIterator<T>{ first.ptr + second.ptr };
    }

    friend RandomAccessIterator<T> operator-(const RandomAccessIterator<T>& first, const RandomAccessIterator<T>& second) {
        return RandomAccessIterator<T>{ first.ptr - second.ptr };
    }

private:
    T* ptr;
};

template<typename T>
RandomAccessIterator<T>::RandomAccessIterator(T* ptr)
    : ptr{ ptr }
{}

template<typename T>
T&& RandomAccessIterator<T>::operator*() const {
    return *ptr;
}

template<typename T>
T& RandomAccessIterator<T>::operator*() {
    return *ptr;
}

template<typename T>
T* RandomAccessIterator<T>::operator->() {
    return ptr;
}

// it++
template<typename T>
RandomAccessIterator<T> RandomAccessIterator<T>::operator++(i32 offset) {
    RandomAccessIterator<T> tmp{ *this };
    ptr += offset;
    return tmp;
}

// ++it
template<typename T>
RandomAccessIterator<T>& RandomAccessIterator<T>::operator++() {
    ++ptr;
    return *this;
}

// it--
template<typename T>
RandomAccessIterator<T> RandomAccessIterator<T>::operator--(i32 offset) {
    RandomAccessIterator<T> tmp{ *this };
    ptr -= offset;
    return tmp;
}

// --it
template<typename T>
RandomAccessIterator<T>& RandomAccessIterator<T>::operator--() {
    --ptr;
    return *this;
}

template<typename T>
RandomAccessIterator<T>& RandomAccessIterator<T>::operator+=(i32 val) {
    ptr += val;
    return *this;
}

template<typename T>
RandomAccessIterator<T>& RandomAccessIterator<T>::operator-=(i32 val) {
    ptr += val;
    return *this;
}

template<typename T>
T& RandomAccessIterator<T>::operator[](usize index) {
    return *(ptr + index);
}

}
