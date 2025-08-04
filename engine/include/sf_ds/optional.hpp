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

    bool is_none() const { return _tag == Tag::NONE; }
    bool is_some() const { return _tag == Tag::SOME; }

    ConstLRefOrValType<Some> unwrap_read() const noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    Some& unwrap_write() noexcept {
        if (_tag == Tag::NONE) {
            panic("Option is none!");
        }
        return _storage.some;
    }

    ConstLRefOrValType<Some> unwrap_or_default_read(RRefOrValType<Some> default_value) const noexcept {
        if (_tag == Tag::NONE) {
            return default_value;
        }
        return _storage.some;
    }

    Some& unwrap_or_default_write(RRefOrValType<Some> default_value) noexcept {
        if (_tag == Tag::NONE) {
            return default_value;
        }
        return _storage.some;
    }

    void set_none() const noexcept {
        _tag = Tag::NONE;
        _storage.none = None::VALUE;
    }
 
    // forward reference needed
    template<typename SomeT>
    requires sf::SameTypes<Some, SomeT>
    void set_some(SomeT&& some_val) const noexcept {
        _tag = Tag::SOME;
        _storage.some = std::forward<Some>(some_val);
    }

    operator bool() const noexcept {
        return this->is_some();
    }
};

} // sf
