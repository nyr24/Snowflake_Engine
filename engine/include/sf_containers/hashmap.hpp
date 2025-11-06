#pragma once

#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_containers/iterator.hpp"
#include "sf_containers/optional.hpp"
#include <cstring>
#include <functional>
#include <string_view>
#include <utility>

namespace sf {

template<typename K>
using HashFn = std::function<u64(ConstLRefOrValType<K>)>;

template<typename K>
using EqualFn = std::function<bool(ConstLRefOrValType<K>, ConstLRefOrValType<K>)>;

template<typename K>
u64 hashfn_default(ConstLRefOrValType<K> key) noexcept;

template<typename K>
bool equal_fn_default(ConstLRefOrValType<K> first, ConstLRefOrValType<K> second) {
    return first == second;
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

template<>
inline u64 hashfn_default<std::string_view>(ConstLRefOrValType<std::string_view> key) noexcept {
    u64 hash = OFFSET_BASIS;

    for (u32 i{0}; i < key.size(); ++i) {
        hash ^= key[i];
        hash *= PRIME;
    }

    return hash;
}

template<>
inline u64 hashfn_default<const char*>(ConstLRefOrValType<const char*> key) noexcept {
    u64 hash = OFFSET_BASIS;

    u32 s_len{ static_cast<u32>(strlen(key)) };

    for (u32 i{0}; i < s_len; ++i) {
        hash ^= key[i];
        hash *= PRIME;
    }

    return hash;
}

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
static HashMapConfig<K> get_default_config() {
    return HashMapConfig<K>{
        hashfn_default<K>,
        equal_fn_default<K>,
        0.8f,
        2.0f,
    };
}

template<typename K, typename V, AllocatorTrait Allocator = GeneralPurposeAllocator>
struct HashMap {
public:
    using KeyType = K;
    using ValueType = V;

    struct Bucket {
        enum BucketState : u8 {
            EMPTY,
            FILLED,
            TOMBSTONE
        };

        K key;
        V value;
        BucketState state;
    };

private:
    // 1 capacity = 1 count = sizeof(Bucket<K, V>)
    Allocator*          _allocator;
    usize               _data;
    u32                 _capacity;
    u32                 _count;
    HashMapConfig<K>    _config;

public:
    HashMap()
        : _allocator{nullptr}
        , _data{INVALID_ALLOC_HANDLE}
        , _capacity{0}
        , _count{0}
        , _config{ get_default_config<K>() }
    {}

    HashMap(Allocator* allocator)
        : _allocator{allocator}
        , _data{INVALID_ALLOC_HANDLE}
        , _capacity{0}
        , _count{0}
        , _config{ get_default_config<K>() }
    {}
    
    HashMap(u32 prealloc_count, Allocator* allocator, const HashMapConfig<K>& config = get_default_config<K>())
        : _allocator{ allocator }
        , _data{ allocator->allocate_handle(prealloc_count * sizeof(Bucket), alignof(Bucket)) }
        , _capacity{ prealloc_count }
        , _count{ 0 }
        , _config{ config }
    {
        init_data_empty(_data, _capacity);
    }

    HashMap(HashMap<K, V, Allocator>&& rhs) noexcept
        : _allocator{ rhs._allocator }
        , _data{ rhs._data }
        , _capacity{ rhs._capacity }
        , _count{ rhs._count }
        , _config{ rhs._config }
    {
        rhs._allocator = nullptr;
        rhs._data = INVALID_ALLOC_HANDLE;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    HashMap<K, V, Allocator>& operator=(HashMap<K, V, Allocator>&& rhs) noexcept
    {
        if (this == &rhs) {
            return *this;
        }

        if (_data != INVALID_ALLOC_HANDLE) {
            _allocator->free_handle(_data);
        }
        
        _allocator = rhs._allocator;
        _data = rhs._data;
        _capacity = rhs._capacity;
        _count = rhs._count;
        _config = rhs._config;

        rhs._allocator = nullptr;
        rhs._data = INVALID_ALLOC_HANDLE;
        rhs._capacity = 0;
        rhs._count = 0;
    }
    
    HashMap(const HashMap<K, V, Allocator>& rhs) = delete;
    HashMap<K, V, Allocator>& operator=(const HashMap<K, V, Allocator>& rhs) = delete;

    ~HashMap() {
        if (_data != INVALID_ALLOC_HANDLE) {
            _allocator->free_handle(_data);
            _data = INVALID_ALLOC_HANDLE;
        }
    }

    void set_allocator(Allocator* alloc) {
        SF_ASSERT_MSG(alloc, "Should be valid pointer");
        _allocator = alloc;
    }

    // updates entry with the same key
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    void put(Key&& key, Val&& val) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");
        if (_count >= static_cast<u32>(_capacity * _config.load_factor)) {
            resize();
        }

        Bucket* data_ptr{ handle_to_ptr(_data) };
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data_ptr[index].state == Bucket::FILLED && !_config.equal_fn(key, data_ptr[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data_ptr[index].state != Bucket::FILLED) {
            data_ptr[index] = Bucket{ .key = std::forward<Key>(key), .value = std::forward<Val>(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            data_ptr[index].value = std::forward<Val>(val);
        }
    }

    // asserts if hashmap needs reallocate buffer
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    bool put_assume_capacity(K&& key, V&& val) noexcept {
        if (_count <= static_cast<u32>(_capacity * _config.load_factor)) {
            LOG_ERROR("Not enough capacity in HashMap");
            return false;
        }

        Bucket* data_ptr{ handle_to_ptr(_data) };
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data_ptr[index].state == Bucket::FILLED && !_config.equal_fn(key, data_ptr[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data_ptr[index].state != Bucket::FILLED) {
            data_ptr[index] = Bucket{ .key = std::move(key), .value = std::move(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            data_ptr[index].value = std::move(val);
        }

        return true;
    }

    // put without update
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    bool put_if_empty(K&& key, V&& val) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");
        if (_count >= static_cast<u32>(_capacity * _config.load_factor)) {
            resize();
        }

        Bucket* data_ptr{ handle_to_ptr(_data) };
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data_ptr[index].state == Bucket::FILLED && !_config.equal_fn(key, data_ptr[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data_ptr[index].state == Bucket::FILLED) {
            return false;
        } else {
            ++_count;
            data_ptr[index] = Bucket{ .key = std::move(key), .value = std::move(val), .state = Bucket::FILLED };
            return true;
        }
    }

    Option<V*> get(ConstLRefOrValType<K> key) noexcept {
        Option<Bucket*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return {None::VALUE};
        }

        return &maybe_bucket.unwrap_ref()->value;
    }

    bool remove(ConstLRefOrValType<K> key) noexcept {
        Option<Bucket*> maybe_bucket = find_bucket(key);
        if (maybe_bucket.is_none()) {
            return false;
        }

        Bucket* bucket = maybe_bucket.unwrap_copy();
        bucket->state = Bucket::TOMBSTONE;
        --_count;

        return true;
    }

    void reserve(u32 new_capacity) noexcept {
        SF_ASSERT_MSG(_allocator, "Allocator should be set");

        if (is_empty()) {
            resize_empty(new_capacity);
        } else {
            resize(new_capacity);
        }
    }

    void fill(ConstLRefOrValType<V> val) noexcept {
        Bucket* data_ptr{ handle_to_ptr(_data) };
        for (u32 i{0}; i < _capacity; ++i) {
            data_ptr[i].value = val;
        }
        _count = _capacity;
    }

    bool is_empty() const { return _data == INVALID_ALLOC_HANDLE && _capacity == 0 && _count == 0; }

    constexpr PtrRandomAccessIterator<Bucket> begin() noexcept {
        return PtrRandomAccessIterator<Bucket>(handle_to_ptr(_data));
    }

    constexpr PtrRandomAccessIterator<Bucket> end() noexcept {
        return PtrRandomAccessIterator<Bucket>(handle_to_ptr(_data) + _capacity);
    }
private:
    constexpr Bucket* handle_to_ptr(usize handle) const {
        return static_cast<Bucket*>(_allocator->get_mem_with_handle(handle));
    }

    void resize_empty(u32 new_capacity) {
        if (new_capacity <= _capacity || _data != INVALID_ALLOC_HANDLE) {
            return;
        }

        Option<usize> maybe_handle = _allocator->reallocate_handle(_data, new_capacity, alignof(Bucket));
        if (maybe_handle.is_none()) {
            LOG_ERROR("Failed to realloc handle: HashMap :: resize_empty");
            return;
        }
        
        _data = maybe_handle.unwrap_copy();
        _capacity = new_capacity;
        init_data_empty(_data, new_capacity);
    }

    void resize(Option<u32> capacity_hint = None::VALUE) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");

        u32 new_capacity;
        u32 grow_factor_capacity = _capacity * _config.grow_factor;

        if (capacity_hint.is_some() && capacity_hint.unwrap_copy() > grow_factor_capacity) {
            new_capacity = capacity_hint.unwrap_copy();
        } else {
            new_capacity = grow_factor_capacity;
        }

        usize maybe_handle = _allocator->reallocate_handle(_data, new_capacity * sizeof(Bucket), alignof(Bucket));
        if (maybe_handle == INVALID_ALLOC_HANDLE) {
            LOG_ERROR("Failed to realloc handle: HashMap :: resize");
            return;
        }
        
        usize new_data = maybe_handle;
        init_data_empty(new_data, new_capacity);

        Bucket* data_ptr{ handle_to_ptr(new_data) };

        for (u32 i{0}; i < _capacity; ++i) {
            Bucket&& bucket{ std::move(data_ptr[i]) };
            if (bucket.state != Bucket::FILLED) {
                continue;
            }

            u64 hash = _config.hash_fn(bucket.key);
            u32 new_index = hash % new_capacity;

            while (data_ptr[new_index].state == Bucket::FILLED) {
                ++new_index;
                new_index %= new_capacity;
            }

            data_ptr[new_index] = std::move(bucket);
        }

        if (new_data != _data) {
            _allocator->free_handle(_data);
        }

        _capacity = new_capacity;
        _data = new_data;
    }

    void init_data_empty(usize new_data, u32 capacity) {
        if (new_data == INVALID_ALLOC_HANDLE) {
            return;
        }

        Bucket* data_ptr = handle_to_ptr(new_data);
        sf_mem_zero(data_ptr, capacity);
    }

    // returns "some" if only state of bucket is "FILLED"
    Option<Bucket*> find_bucket(ConstLRefOrValType<K> key) noexcept {
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;
        u32 search_count = 0;
        Bucket* data_ptr{ handle_to_ptr(_data) };

        while (data_ptr[index].state == Bucket::FILLED && !_config.equal_fn(key, handle_to_ptr(_data)[index].key) && search_count < _capacity) {
            ++index;
            index %= _capacity;
            ++search_count;
        }

        if (data_ptr[index].state == Bucket::FILLED && search_count < _capacity) {
            return &data_ptr[index];
        } else {
            return None::VALUE;
        }
    }
};

} // sf
