#pragma once
#include "sf_core/defines.hpp"
#include "sf_core/utility.hpp"
#include <concepts>
#include <numeric>
#include <utility>

namespace sf {

enum struct None {
    VALUE
};

template<typename Some>
struct Option {
private:
    union Storage {
        None none;
        Some some;

        ~Storage() noexcept {}
    };

    enum struct Tag : u8 {
        NONE,
        SOME
    };

    Storage _storage;
    Tag     _tag;

public:
    Option(None none_val)
        : _storage{ .none = none_val }
        , _tag{ Tag::NONE }
    {}

    Option(RRefOrValType<Some> some_val)
        : _storage{ .some = std::move(some_val) }
        , _tag{ Tag::SOME }
    {}

    Option(const Option<Some>& rhs) noexcept
        : _storage{ rhs._storage }
        , _tag{ rhs._tag }
    {}

    Option& operator=(const Option<Some>& rhs) noexcept
    {
        if (this == &rhs) {
            return *this;
        }
        
        _storage = rhs._storage;
        _tag = rhs._tag;
        return *this;
    }

    Option(Option<Some>&& rhs) noexcept
        : _storage{ std::move(rhs._storage) }
        , _tag{ rhs._tag }
    {}

    Option& operator=(Option<Some>&& rhs) noexcept
    {
        if (this == &rhs) {
            return *this;
        }
        
        _storage = std::move(rhs._storage);
        _tag = rhs._tag;

        rhs._storage = None::VALUE;
        rhs._tag = Tag::NONE;
        
        return *this;
    }

    friend bool operator==(Option<Some>& first, Option<Some>& sec) {
        if (first._tag != sec._tag) {
            return false;
        } else if (first._tag == Tag::NONE) {
            return true;
        } else {
            if constexpr (std::equality_comparable<Some>) {
                return first._storage == sec._storage;
            } else {
                return true;
            }
        }
    }

    ~Option() noexcept {
        if (_tag == Tag::SOME) {
            _storage.some.~Some();
        }
    }

    bool is_none() const { return _tag == Tag::NONE; }
    bool is_some() const { return _tag == Tag::SOME; }

    const Some& unwrap() const noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    Some& unwrap() noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    Some unwrap_copy() const noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    Some&& unwrap_move() noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return std::move(_storage.some);
    }

    ConstLRefOrVal<Some> unwrap_or_default(ConstLRefOrVal<Some> default_val) const noexcept {
        if (_tag == Tag::NONE) {
            return default_val;
        }

        return _storage.some;
    }

    void set_none() noexcept {
        _tag = Tag::NONE;
        _storage.none = None::VALUE;
    }
 
    void set_some(RRefOrValType<Some> some_val) noexcept {
        _tag = Tag::SOME;
        _storage.some = std::move(some_val);
    }
};

} // sf
