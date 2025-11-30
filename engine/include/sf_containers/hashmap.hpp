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
#include <string_view>
#include <utility>

namespace sf {

template<typename K>
using HashFn = u64(*)(ConstLRefOrValType<K>);

template<typename K>
using EqualFn = bool(*)(ConstLRefOrValType<K>, ConstLRefOrValType<K>);

template<typename K>
u64 hashfn_default(ConstLRefOrValType<K> key) noexcept;

template<typename K>
bool equal_fn_default(ConstLRefOrValType<K> first, ConstLRefOrValType<K> second) {
    return first == second;
};

template<>
inline bool equal_fn_default<std::string_view>(ConstLRefOrValType<std::string_view> first, ConstLRefOrValType<std::string_view> second) {
    if (first.size() != second.size()) {
        return false;
    }
    return sf_mem_cmp((void*)first.data(), (void*)second.data(), first.size());
};

template<>
inline bool equal_fn_default<const char*>(ConstLRefOrValType<const char*> first, ConstLRefOrValType<const char*> second) {
    const u32 first_len = strlen(first);
    if (first_len != strlen(second)) {
        return false;
    }
    return sf_mem_cmp((void*)first, (void*)second, first_len);
};

// fnv1a hash function
static constexpr u64 PRIME = 1099511628211ull;
static constexpr u64 OFFSET_BASIS = 14695981039346656037ull;

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

template<typename K, typename V, AllocatorTrait Allocator = GeneralPurposeAllocator, bool USE_HANDLE = true, u32 DEFAULT_INIT_CAPACITY = 8>
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

    union Data {
        Bucket* ptr;
        u32     handle; 
    };
private:
    // 1 capacity = 1 count = sizeof(Bucket<K, V>)
    Allocator*          _allocator;
    Data                _data;
    u32                 _capacity;
    u32                 _count;
    HashMapConfig<K>    _config;

public:
    HashMap()
        : _allocator{nullptr}
        , _capacity{0}
        , _count{0}
        , _config{ get_default_config<K>() }
    {
        if constexpr (USE_HANDLE) {
            _data.handle = INVALID_ALLOC_HANDLE;
        } else {
            _data.ptr = nullptr;
        }
    }

    HashMap(Allocator* allocator)
        : _allocator{allocator}
        , _capacity{0}
        , _count{0}
        , _config{ get_default_config<K>() }
    {
        if constexpr (USE_HANDLE) {
            _data.handle = INVALID_ALLOC_HANDLE;
        } else {
            _data.ptr = nullptr;
        }
    }
    
    HashMap(u32 prealloc_count, Allocator* allocator, const HashMapConfig<K>& config = get_default_config<K>())
        : _allocator{allocator}
        , _capacity{prealloc_count}
        , _count{0}
        , _config{config}
    {
        init_buffer_empty(access_data(), _capacity);
    }

    HashMap(HashMap<K, V, Allocator>&& rhs) noexcept
        : _allocator{rhs._allocator}
        , _data{rhs._data}
        , _capacity{rhs._capacity}
        , _count{rhs._count}
        , _config{rhs._config}
    {
        rhs._allocator = nullptr;
        if constexpr (USE_HANDLE) {
            rhs._data.handle = INVALID_ALLOC_HANDLE;
        } else {
            rhs._data.ptr = nullptr;
        }
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }

    HashMap<K, V, Allocator>& operator=(HashMap<K, V, Allocator>&& rhs) noexcept
    {
        if (this == &rhs) {
            return *this;
        }

        Bucket* data = access_data();
        if (_allocator && data) {
            _allocator->free(data);
        }
        
        _allocator = rhs._allocator;
        _data = rhs._data;
        _capacity = rhs._capacity;
        _count = rhs._count;
        _config = rhs._config;

        rhs._allocator = nullptr;
        if constexpr (USE_HANDLE) {
            rhs._data.handle = INVALID_ALLOC_HANDLE;
        } else {
            rhs._data.ptr = nullptr;
        }
        rhs._buffer = nullptr;
        rhs._capacity = 0;
        rhs._count = 0;
    }
    
    HashMap(const HashMap<K, V, Allocator>& rhs) = delete;
    HashMap<K, V, Allocator>& operator=(const HashMap<K, V, Allocator>& rhs) = delete;

    ~HashMap() noexcept {
        free();
    }

    void free() noexcept {
        if constexpr (USE_HANDLE) {
            if (_data.handle != INVALID_ALLOC_HANDLE) {
                _allocator->free_handle(_data.handle);
                _data.handle = INVALID_ALLOC_HANDLE;
            }
        } else {
            if (_data.ptr) {
                _allocator->free(_data.ptr);
                _data.ptr = nullptr;
            }
        }
    }

    void set_allocator(Allocator* alloc) noexcept {
        SF_ASSERT_MSG(alloc, "Should be valid pointer");
        _allocator = alloc;
    }

    // updates entry with the same key
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    void put(Key&& key, Val&& val) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");
        if (_count > static_cast<u32>(_capacity * _config.load_factor)) {
            resize(_capacity * _config.grow_factor);
        }

        Bucket* data = access_data();
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data[index].state == Bucket::FILLED && !_config.equal_fn(key, data[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data[index].state != Bucket::FILLED) {
            data[index] = Bucket{ .key = std::forward<Key>(key), .value = std::forward<Val>(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            data[index].value = std::forward<Val>(val);
        }
    }

    // put without resize, if can
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    bool put_assume_capacity(Key&& key, Val&& val) noexcept {
        Bucket* data = access_data();
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data[index].state == Bucket::FILLED && !_config.equal_fn(key, data[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data[index].state != Bucket::FILLED) {
            if (_count > static_cast<u32>(_capacity * _config.load_factor)) {
                LOG_ERROR("Not enough capacity in HashMap");
                return false;
            }
            data[index] = Bucket{ .key = std::forward<Key>(key), .value = std::forward<Val>(val), .state = Bucket::FILLED };
            ++_count;
        } else {
            data[index].value = std::move(val);
        }

        return true;
    }

    // put without update
    template<typename Key, typename Val>
    requires SameTypes<K, Key> && SameTypes<V, Val>
    bool put_if_empty(Key&& key, Val&& val) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");
        if (_count > static_cast<u32>(_capacity * _config.load_factor)) {
            resize(_capacity * _config.grow_factor);
        }

        Bucket* data = access_data();
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;

        while (data[index].state == Bucket::FILLED && !_config.equal_fn(key, data[index].key)) {
            ++index;
            index %= _capacity;
        }

        if (data[index].state == Bucket::FILLED) {
            return false;
        } else {
            ++_count;
            data[index] = Bucket{ .key = std::forward<Key>(key), .value = std::forward<Val>(val), .state = Bucket::FILLED };
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
        Bucket* data = access_data();
        for (u32 i{0}; i < _capacity; ++i) {
            data[i].value = val;
        }
        _count = _capacity;
    }

    bool is_empty() const {
        if constexpr (USE_HANDLE) {
            return _data.handle == INVALID_ALLOC_HANDLE && _capacity == 0 && _count == 0;
        } else {
            return _data.ptr == nullptr && _capacity == 0 && _count == 0;
        }
    }

    constexpr PtrRandomAccessIterator<Bucket> begin() noexcept {
        return PtrRandomAccessIterator<Bucket>(access_data());
    }

    constexpr PtrRandomAccessIterator<Bucket> end() noexcept {
        return PtrRandomAccessIterator<Bucket>(access_data() + _capacity);
    }
private:
    Bucket* access_data() {
        if constexpr (USE_HANDLE) {
            return static_cast<Bucket*>(_allocator->handle_to_ptr(_data.handle));
        } else {
            return _data.ptr;
        }
    }

    void resize_empty(u32 new_capacity) {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");

        _capacity = DEFAULT_INIT_CAPACITY;
        while (_capacity < new_capacity) {
            _capacity *= _config.grow_factor;
        }
        if constexpr (USE_HANDLE) {
            _data.handle = _allocator->allocate_handle(_capacity * sizeof(Bucket), alignof(Bucket));
            init_buffer_empty(_data.ptr, _capacity);
        } else {
            _data.ptr = static_cast<Bucket*>(_allocator->allocate(_capacity * sizeof(Bucket), alignof(Bucket)));
            init_buffer_empty(_data.ptr, _capacity);
        }
    }

    void resize(u32 new_capacity) noexcept {
        SF_ASSERT_MSG(_allocator, "Should be valid pointer");

        u32 old_capacity = _capacity;
        if (_capacity == 0) {
            _capacity = DEFAULT_INIT_CAPACITY;
        }
        while (_capacity < new_capacity) {
            _capacity *= _config.grow_factor;
        }

        Bucket* old_buffer = access_data();
        ReallocReturn realloc_res = _allocator->reallocate(static_cast<void*>(old_buffer), _capacity * sizeof(Bucket), alignof(Bucket));
        Bucket* new_buffer = static_cast<Bucket*>(realloc_res.ptr);
        if (realloc_res.should_mem_copy && old_buffer != new_buffer && old_capacity > 0) {
            sf_mem_copy((void*)new_buffer, (void*)old_buffer, old_capacity * sizeof(Bucket));
        }

        // copy old nodes
        for (u32 i{0}; i < _capacity; ++i) {
            Bucket&& bucket{ std::move(new_buffer[i]) };
            if (bucket.state != Bucket::FILLED) {
                bucket.state = Bucket::EMPTY;
                continue;
            }

            u64 hash = _config.hash_fn(bucket.key);
            u32 new_index = hash % _capacity;

            while (new_buffer[new_index].state == Bucket::FILLED) {
                ++new_index;
                new_index %= _capacity;
            }

            new_buffer[new_index] = std::move(bucket);
        }

        if constexpr (USE_HANDLE) {
            _data.handle = _allocator->ptr_to_handle(new_buffer);
        } else {
            _data.ptr = new_buffer;
        }
    }

    void init_buffer_empty(Bucket* new_buffer, u32 capacity) {
        if (!new_buffer) {
            return;
        }

        sf_mem_zero(new_buffer, capacity * sizeof(Bucket));
    }

    // returns "some" if only state of bucket is "FILLED"
    Option<Bucket*> find_bucket(ConstLRefOrValType<K> key) noexcept {
        u32 index = static_cast<u32>(_config.hash_fn(key)) % _capacity;
        u32 search_count = 0;

        Bucket* data = access_data();
        while (data[index].state == Bucket::FILLED && !_config.equal_fn(key, data[index].key) && search_count < _capacity) {
            ++index;
            index %= _capacity;
            ++search_count;
        }

        if (data[index].state == Bucket::FILLED && search_count < _capacity) {
            return &data[index];
        } else {
            return None::VALUE;
        }
    }
};

} // sf
