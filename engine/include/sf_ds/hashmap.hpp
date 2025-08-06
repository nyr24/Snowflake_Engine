#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_ds/iterator.hpp"
#include "sf_ds/optional.hpp"
#include "sf_core/asserts_sf.hpp"
#include <functional>
#include <utility>

namespace sf {

template<typename K, typename V>
struct HashMap;

template<typename K>
using HashFn = std::function<u64(ConstLRefOrValType<K>)>;

template<typename K>
using EqualFn = std::function<bool(ConstLRefOrValType<K>, ConstLRefOrValType<K>)>;

template<typename K>
u64 hashfn_default(ConstLRefOrValType<K> key) noexcept;

template<typename K>
EqualFn<K> equal_fn_default = [](ConstLRefOrValType<K> first, ConstLRefOrValType<K> second) -> bool {
    return first == second;
};

template<typename K>
struct HashMapConfig {
    HashFn<K>   hash_fn;
    EqualFn<K>  equal_fn;
    f32         load_factor;
    f32         grow_factor;

    HashMapConfig(
        HashFn<K> hash_fn = hashfn_default<K>,
        EqualFn<K> equal_fn = equal_fn_default<K>,
        f32 load_factor = 0.8f,
        f32 grow_factor = 2.0f
    )
        : hash_fn{ hash_fn }
        , equal_fn{ equal_fn }
        , load_factor{ load_factor }
        , grow_factor{ grow_factor }
    {}
};

template<typename K>
static HashMapConfig<K> map_default_config {
    hashfn_default<K>,
    equal_fn_default<K>,
    0.8f,
    2.0f,
};

template<typename K, typename V>
struct HashMap {
public:
    using KeyType = K;
    using ValueType = V;

    struct Bucket {
        enum BucketState : u8 {
            FILLED,
            EMPTY,
            TOMBSTONE
        };

        K key;
        V value;
        BucketState state;
    };

private:
    // 1 capacity = 1 count = sizeof(Bucket<K, V>)
    u32                 _capacity;
    u32                 _count;
    Bucket*             _buffer;
    HashMapConfig<K>    _config = map_default_config<K>;

public:
    HashMap(u64 prealloc_count, const HashMapConfig<K>& config = map_default_config<K>)
        : _capacity(static_cast<u32>(prealloc_count))
        , _count{ 0 }
        , _buffer{ static_cast<Bucket*>(sf_mem_alloc_typed<Bucket, true>(prealloc_count)) }
        , _config{ std::move(config) }
    {
        init_buffer_empty(_buffer, _capacity);
    }

    // updates entry with the same key
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    void put(Key&& key, Val&& val) noexcept {
        if (_count >= static_cast<u32>(_capacity * _config.load_factor)) {
            realloc();
        }

        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (_buffer[index].state == Bucket::FILLED && !_config.equal_fn(key, _buffer[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (_buffer[index].state != Bucket::FILLED) {
            _buffer[index] = Bucket{ .key = std::forward<Key>(key), .value = std::forward<Val>(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            _buffer[index].value = std::forward<Val>(val);
        }
    }

    // asserts if hashmap needs reallocate buffer
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    void put_assume_capacity(K&& key, V&& val) noexcept {
        SF_ASSERT_MSG(_count <= (static_cast<u32>(_capacity * _config.load_factor)), "Not enough capacity in HashMap");

        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (_buffer[index].state == Bucket::FILLED && !_config.equal_fn(key, _buffer[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (_buffer[index].state != Bucket::FILLED) {
            _buffer[index] = Bucket{ .key = std::move(key), .value = std::move(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            _buffer[index].value = std::move(val);
        }
    }

    // put without update
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    bool put_if_empty(K&& key, V&& val) noexcept {
        if (_count >= static_cast<u32>(_capacity * _config.load_factor)) {
            realloc();
        }

        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (_buffer[index].state == Bucket::FILLED && !_config.equal_fn(key, _buffer[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (_buffer[index].state == Bucket::FILLED) {
            return false;
        } else {
            ++_count;
            _buffer[index] = Bucket{ .key = std::move(key), .value = std::move(val), .state = Bucket::FILLED };
            return true;
        }
    }

    Option<V> get(ConstLRefOrValType<K> key) noexcept {
        Option<Bucket*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return None::VALUE;
        }

        return maybe_bucket.unwrap()->value;
    }

    bool remove(ConstLRefOrValType<K> key) noexcept {
        Option<Bucket*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return false;
        }

        Bucket* bucket = maybe_bucket.unwrap();
        bucket->state = Bucket::TOMBSTONE;
        --_count;

        return true;
    }

    constexpr PtrRandomAccessIterator<Bucket> begin() noexcept {
        return PtrRandomAccessIterator<Bucket>(_buffer);
    }

    constexpr PtrRandomAccessIterator<Bucket> end() noexcept {
        return PtrRandomAccessIterator<Bucket>(_buffer + _capacity);
    }
private:
    void realloc() noexcept {
        u32 new_capacity = _capacity * _config.grow_factor;
        Bucket* new_buffer = static_cast<Bucket*>(sf_mem_realloc_typed<Bucket>(new_capacity));
        init_buffer_empty(new_buffer, new_capacity);

        for (u32 i{0}; i < _capacity; ++i) {
            Bucket&& bucket{ std::move(_buffer[i]) };
            if (bucket.state != Bucket::FILLED) {
                continue;
            }

            u64 hash = _config.hash_fn(bucket.key);
            u32 new_index = hash % _capacity;

            while (new_buffer[new_index].state == Bucket::FILLED) {
                ++new_index;
                new_index %= new_capacity;
            }

            new_buffer[new_index] = std::move(bucket);
        }

        if (new_buffer != _buffer) {
            sf_mem_free_typed<Bucket, true>(_buffer);
        }

        _capacity = new_capacity;
        _buffer = new_buffer;
    }

    void init_buffer_empty(Bucket* buffer, u32 capacity) {
        if (!buffer) {
            return;
        }

        for (u16 i{0}; i < capacity; ++i) {
            buffer[i].state = Bucket::EMPTY;
        }
    }

    // returns "some" if only state of bucket is "FILLED"
    Option<Bucket*> find_bucket(ConstLRefOrValType<K> key) noexcept {
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;
        u32 search_count = 0;

        while ((_buffer[index].state != Bucket::FILLED && !_config.equal_fn(key, _buffer[index].key)) && search_count < _capacity) {
            ++index;
            index %= _capacity;
            ++search_count;
        }

        if (_buffer[index].state == Bucket::FILLED && search_count < _capacity) {
            return _buffer + index;
        } else {
            return None::VALUE;
        }
    }
};

// fnv1a hash function
static constexpr u64 PRIME = 1099511628211u;
static constexpr u64 OFFSET_BASIS = 14695981039346656037u;

template<typename K>
u64 hashfn_default(ConstLRefOrValType<K> key) noexcept {
    u64 hash = OFFSET_BASIS;

    u8 bytes[sizeof(K)];
    sf_mem_copy(bytes, &key, sizeof(K));

    for (u32 i{0}; i < sizeof(K); ++i) {
        hash ^= bytes[i];
        hash *= PRIME;
    }

    return hash;
}

} // sf
