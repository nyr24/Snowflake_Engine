#pragma once
#include "sf_core/defines.hpp"
#include "sf_core/utility.hpp"

namespace sf {

template<typename OkType, typename ErrorType>
struct ResultMultiError {
private:
    union Storage {
        ErrorType err;
        OkType ok;

        ~Storage() noexcept {}
    };

    enum struct Tag : u8 {
        ERROR,
        OK
    };

    Storage _storage;
    Tag     _tag;

public:
    ResultMultiError(RRefOrValType<OkType> ok_val)
        : _storage{ .ok = std::move(ok_val)}
        , _tag{ Tag::OK }
    {}

    ResultMultiError(ErrorType error)
        : _storage{ .err = error }
        , _tag{ Tag::ERROR }
    {}

    ResultMultiError(const ResultMultiError<OkType, ErrorType>& rhs) noexcept
        : _storage{ rhs._storage }
        , _tag{ rhs._tag }
    {}

    ResultMultiError(ResultMultiError<OkType, ErrorType>&& rhs) noexcept
        : _storage{ std::move(rhs._storage) }
        , _tag{ rhs._tag }
    {}

    ~ResultMultiError() noexcept {
        if (_tag == Tag::OK) {
            _storage.ok.~OkType();
        }
    }

    bool is_err() const { return _tag == Tag::ERROR; }
    bool is_ok() const { return _tag == Tag::OK; }

    const OkType& unwrap() const noexcept {
        if (_tag == Tag::ERROR) {
            panic("Result is error!");
        }
        return _storage.ok;
    }

    OkType& unwrap() noexcept {
        if (_tag == Tag::ERROR) {
            panic("Result is error!");
        }
        return _storage.ok;
    }

    OkType unwrap_copy() const noexcept {
        if (_tag == Tag::ERROR) {
            panic("Result is error!");
        }
        return _storage.ok;
    }

    OkType&& unwrap_move() noexcept {
        if (_tag == Tag::ERROR) {
            panic("Result is error!");
        }
        return _storage.ok;
    }

    ConstLRefOrValType<OkType> unwrap_or_default(ConstLRefOrValType<OkType> default_value) const noexcept {
        if (_tag == Tag::ERROR) {
            return default_value;
        }
        return _storage.ok;
    }

    void set_err(RRefOrValType<ErrorType> err) const noexcept {
        _tag = Tag::ERROR;
        _storage.err = std::move(err);
    }
 
    void set_ok(RRefOrValType<OkType> ok_val) const noexcept {
        _tag = Tag::OK;
        _storage.some = std::move(ok_val);
    }

    operator bool() const noexcept {
        return this->is_ok();
    }
};

enum struct ResultError {
    VALUE
};

template<typename OkType>
using Result = ResultMultiError<OkType, ResultError>;

} // sf
