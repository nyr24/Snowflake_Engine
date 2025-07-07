#pragma once

#include "sf_core/memory_sf.hpp"
#include "sf_core/types.hpp"
#include "sf_core/utility.hpp"
#include "sf_ds/optional.hpp"

namespace sf {

template<typename K, typename V>
struct HashMap {
public:
    struct Bucket {
        K key;
        V value;
    };

    struct Config {
        f32 load_factor;
        f32 grow_factor;
    };

private:
    // 1 capacity = 1 count = sizeof(Bucket<K, V>)
    usize               _capacity;
    usize               _count;
    Option<Bucket>*     _buffer;
    Config              _config;

public:
    HashMap(u64 prealloc_count, Config config)
        : _capacity(static_cast<usize>(prealloc_count))
        , _count{ 0 }
        , _buffer{ static_cast<Option<Bucket>*>(sf_mem_alloc(sizeof(Option<Bucket>) * prealloc_count)) }
        , _config{ config }
    {
        init_buffer(_buffer);
    }

    // updates entry with the same key
    void put(K&& key, V&& val) noexcept {
        if (_count > static_cast<usize>(_capacity * _config.load_factor)) {
            realloc();
        }

        usize index = static_cast<usize>(hash_fnv1a(key)) % _capacity;

        while (_buffer[index].is_some() && key != _buffer[index].unwrap().key) {
            ++index;
            index %= _capacity;
        }

        _buffer[index].set_some(Bucket{ .key = std::move(key), .value = std::move(val) });
        ++_count;
    }

    // put without update
    bool put_if_empty(K&& key, V&& val) noexcept {
        if (_count > static_cast<usize>(_capacity * _config.load_factor)) {
            realloc();
        }

        usize index = static_cast<usize>(hash_fnv1a(key)) % _capacity;

        while (_buffer[index].is_some() && key != _buffer[index].unwrap().key) {
            ++index;
            index %= _capacity;
        }

        if (_buffer[index].is_some()) {
            return false;
        }

        _buffer[index].set_some(Bucket{ .key = key, .value = val });
        ++_count;
        return true;
    }

    Option<V> get(ConstRefOrVal<K>::Type key) noexcept {
        Option<Option<Bucket>*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return None::Value;
        }

        return maybe_bucket.unwrap()->unwrap().value;
    }

    bool remove(ConstRefOrVal<K>::Type key) noexcept {
        Option<Option<Bucket>*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return false;
        }

        maybe_bucket.unwrap()->set_none();
        return true;
    }

private:
    static constexpr u64 PRIME = 1099511628211u;
    static constexpr u64 OFFSET_BASIS = 14695981039346656037u;

    u64 hash_fnv1a(ConstRefOrVal<K>::Type key) noexcept {
        u64 hash = OFFSET_BASIS;

        u8 bytes[sizeof(K)];
        sf_mem_copy(bytes, &key, sizeof(K));
 
        for (u32 i{0}; i < sizeof(K); ++i) {
            hash ^= bytes[i];
            hash *= PRIME;
        }

        return hash;
    }

    void realloc() noexcept {
        usize new_capacity = static_cast<usize>(_capacity * _config.grow_factor);
        Option<Bucket>* new_buffer = static_cast<Option<Bucket>*>(sf_mem_alloc<Option<Bucket>>(new_capacity));
        init_buffer(new_buffer);

        for (usize i{0}; i < _capacity; ++i) {
            Option<Bucket>&& bucket_opt{ std::move(_buffer[i]) };
            if (bucket_opt.is_none()) {
                continue;
            }

            u64 new_hash = hash_fnv1a(bucket_opt.unwrap().key);
            usize index = new_hash % new_capacity;

            new_buffer[index].set_some(bucket_opt.unwrap());
        }

        sf_mem_free<Option<Bucket>>(_buffer);

        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    void init_buffer(Option<Bucket>* buffer) {
        if (!buffer) {
            return;
        }

        for (u16 i{0}; i < _capacity; ++i) {
            buffer[i].set_none();
        }
    }

    // inner option - bucket can be empty
    // outer option - bucket with input key was not found
    Option<Option<Bucket>*> find_bucket(ConstRefOrVal<K>::Type key) noexcept {
        usize index = static_cast<usize>(hash_fnv1a(key)) % _capacity;
        usize search_count = 0;

        while (_buffer[index].is_some() && key != _buffer[index].unwrap().key && search_count <= _capacity) {
            ++index;
            index %= _capacity;
            ++search_count;
        }

        if (_buffer[index].is_none() || search_count >= _capacity) {
            return None::Value;
        } else {
            return &_buffer[index];
        }
    }
};

} // sf
