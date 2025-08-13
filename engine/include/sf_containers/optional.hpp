#pragma once
#include "sf_core/defines.hpp"
#include "sf_core/utility.hpp"

namespace sf {

enum struct None {
    VALUE
};

template<typename Some>
struct Option {
public:
    union Storage {
        None none;
        Some some;
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

    Option(Option<Some>&& rhs) noexcept
        : _storage{ std::move(rhs._storage) }
        , _tag{ rhs._tag }
    {}

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

    void set_none() noexcept {
        _tag = Tag::NONE;
        _storage.none = None::VALUE;
    }
 
    void set_some(RRefOrValType<Some> some_val) noexcept {
        _tag = Tag::SOME;
        _storage.some = std::move(some_val);
    }

    operator bool() const noexcept {
        return this->is_some();
    }
};

} // sf
