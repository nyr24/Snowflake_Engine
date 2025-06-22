#pragma once
#include "sf_core/types.hpp"
#include <cstddef>

namespace sf {

// RandomAccessIterator
template<typename T>
struct RandomAccessIterator {
private:
    T* ptr;

public:
    using ValueType = T;
    using Ptr = T*;
    using LvalueRef = T&;
    using RvalueRef = T&&;
    using ConstRef = const T&;
    using Diff = std::ptrdiff_t;

    RandomAccessIterator() = default;
    ~RandomAccessIterator() = default;
    RandomAccessIterator(const RandomAccessIterator<T>& rhs) = default;
    RandomAccessIterator<T>& operator=(const RandomAccessIterator<T>& rhs) = default;

    RandomAccessIterator(T* ptr)
        : ptr{ ptr }
    {}

    T&& operator*() const {
        return *ptr;
    }

    T& operator*() {
        return *ptr;
    }

    T* operator->() {
        return ptr;
    }

    // it++
    RandomAccessIterator<T> operator++(i32 offset) {
        RandomAccessIterator<T> tmp{ *this };
        ptr += offset;
        return tmp;
    }

    // ++it
    RandomAccessIterator<T>& operator++() {
        ++ptr;
        return *this;
    }

    // it--
    RandomAccessIterator<T> operator--(i32 offset) {
        RandomAccessIterator<T> tmp{ *this };
        ptr -= offset;
        return tmp;
    }

    // --it
    RandomAccessIterator<T>& operator--() {
        --ptr;
        return *this;
    }

    RandomAccessIterator<T>& operator+=(i32 val) {
        ptr += val;
        return *this;
    }

    RandomAccessIterator<T>& operator-=(i32 val) {
        ptr += val;
        return *this;
    }

    T& operator[](usize index) {
        return *(ptr + index);
    }

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
};

} // sf
