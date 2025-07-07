#pragma once
#include "sf_core/utility.hpp"

namespace sf {

enum struct None {
    Value
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

    Option(Some some_val)
        : _storage{ .some = some_val }
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

    bool is_none() { return _tag == Tag::NONE; }
    bool is_some() { return _tag == Tag::SOME; }

    // optimized function, if size <= size of word return by value, else by reference
    RefOrVal<Some>::Type unwrap() noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    Some& unwrap_ref() noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    void set_none() {
        _tag = Tag::NONE;
        _storage.none = None::Value;
    }
 
    // forward reference needed
    template<typename SomeT>
    requires sf::SameTypes<Some, SomeT>
    void set_some(SomeT&& some_val) {
        _tag = Tag::SOME;
        _storage.some = std::forward<Some>(some_val);
    }
};

} // sf
