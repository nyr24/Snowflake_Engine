#pragma once
#include "sf_core/types.hpp"
#include <cstddef>
#include <iterator>

namespace sf {

template<typename T>
struct PtrForwardIterator {
protected:
    T* ptr;
public:
    using ValueType = T;
    using Ptr = T*;
    using LvalueRef = T&;
    using RvalueRef = T&&;
    using ConstRef = const T&;
    using Diff = std::ptrdiff_t;
    using Strategy = std::forward_iterator_tag;

    PtrForwardIterator() = default;
    ~PtrForwardIterator() = default;
    PtrForwardIterator(const PtrForwardIterator<T>& rhs) = default;
    PtrForwardIterator<T>& operator=(const PtrForwardIterator<T>& rhs) = default;

    PtrForwardIterator(T* ptr)
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
    PtrForwardIterator<T> operator++(i32 offset) {
        PtrForwardIterator<T> tmp{ *this };
        ++ptr;
        return tmp;
    }

    // ++it
    PtrForwardIterator<T>& operator++() {
        ++ptr;
        return *this;
    }

    friend bool operator==(const PtrForwardIterator<T>& first, const PtrForwardIterator<T>& second) {
        return first.ptr == second.ptr;
    }

    friend bool operator!=(const PtrForwardIterator<T>& first, const PtrForwardIterator<T>& second) {
        return first.ptr != second.ptr;
    }
};

template<typename T>
struct PtrBidirectionalOperator : public PtrForwardIterator<T> {
public:
    using Strategy = std::bidirectional_iterator_tag;
    using PtrForwardIterator<T>::PtrForwardIterator;

    // it--
    PtrBidirectionalOperator<T> operator--(i32 offset) {
        PtrBidirectionalOperator<T> tmp{ *this };
        this->ptr -= offset;
        return tmp;
    }

    // --it
    PtrBidirectionalOperator<T>& operator--() {
        --this->ptr;
        return *this;
    }
};

template<typename T>
struct PtrRandomAccessIterator : public PtrBidirectionalOperator<T> {
public:
    using Strategy = std::random_access_iterator_tag;
    using PtrBidirectionalOperator<T>::PtrBidirectionalOperator;

    T& operator[](usize index) {
        return *(this->ptr + index);
    }

    PtrRandomAccessIterator<T>& operator+=(i32 val) {
        this->ptr += val;
        return *this;
    }

    PtrRandomAccessIterator<T>& operator-=(i32 val) {
        this->ptr += val;
        return *this;
    }

    friend PtrRandomAccessIterator<T> operator-(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return PtrRandomAccessIterator<T>{ first.ptr - second.ptr };
    }

    friend PtrRandomAccessIterator<T> operator+(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return PtrRandomAccessIterator<T>{ first.ptr + second.ptr };
    }

    friend bool operator>(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return first.ptr > second.ptr;
    }

    friend bool operator<(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return first.ptr < second.ptr;
    }

    friend bool operator>=(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return first.ptr >= second.ptr;
    }

    friend bool operator<=(const PtrRandomAccessIterator<T>& first, const PtrRandomAccessIterator<T>& second) {
        return first.ptr <= second.ptr;
    }
};

} // sf
