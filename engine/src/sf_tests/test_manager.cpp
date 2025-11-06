#ifdef SF_TESTS

#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/hashmap.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_core/logger.hpp"
#include "sf_tests/test_manager.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_allocators/free_list_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_core/clock.hpp"
#include "sf_containers/dynamic_array_self_managed.hpp"
#include <string_view>

namespace sf {

void TestManager::add_test(TestFn test) { 
    module_tests.append(test);
}

void TestManager::run_all_tests() const {
    for (const auto& m_test : module_tests) {
        m_test();
    }
}

TestCounter::TestCounter(std::string_view name)
    : test_name{ name }
{
    LOG_INFO("Tests for {} started: ", name);
}

TestCounter::~TestCounter() {
    LOG_DEBUG("Tests for {} ended:\t\n{} all, passed: {}, failed: {}", test_name, all, passed, failed);
}

// Tests

void fixed_array_test() {
    TestCounter counter("Fixed Array");
    FixedArray<const char*, 10> arr{ "hello", "world", "crazy", "boy" };

    expect(arr.capacity() == 10u, counter);
    expect(arr.count() == 4u, counter);

    arr.append_emplace("many");
    arr.append_emplace("items");

    expect(arr.capacity() == 10u, counter);
    expect(arr.count() == 6u, counter);

    arr.remove_unordered_at(5);
    arr.remove_unordered_at(4);

    expect(arr.capacity() == 10u, counter);
    expect(arr.count() == 4u, counter);
}

void dyn_array_test() {
    Clock clock;

    constexpr u32 BIG_SIZE = 99999999;

    {
        GeneralPurposeAllocator al;
        DynamicArray<u32> arr(32, &al);
        clock.restart();
        for (u32 i{0}; i < BIG_SIZE; ++i) {
            arr.append(rand());
        }
        auto time = clock.update_and_get_delta();
        LOG_FATAL("new dyn array: {}", time);
    }

    {
        DynamicArraySelfManaged<u32> arr(32);
        clock.restart();
        for (u32 i{0}; i < BIG_SIZE; ++i) {
            arr.append(rand());
        }
        auto time = clock.update_and_get_delta();
        LOG_FATAL("dyn array: {}", time);
    }
}

void stack_allocator_test() {
    TestCounter counter("Stack Allocator");
    StackAllocator alloc{500};

    {
        DynamicArray<u8, StackAllocator> arr(&alloc);
        arr.reserve(200);
        expect(alloc.count() >= 200 * sizeof(u8) + sizeof(StackAllocatorHeader), counter);

        DynamicArray<u8, StackAllocator> arr2(&alloc);
        arr2.reserve(200);
        expect(alloc.count() >= 400 * sizeof(u8) + sizeof(StackAllocatorHeader), counter);

        DynamicArray<u8, StackAllocator> arr3(&alloc);
        arr3.reserve(300);
        expect(alloc.count() >= 700 * sizeof(u8) + sizeof(StackAllocatorHeader), counter);
    }
}

void freelist_allocator_test() {
    FreeList<{ true, true }> list(1024);
    u32 h1 = list.allocate_handle(448, 8);
    u32 h3 = list.allocate_handle(224, 8);
    u32 h2 = list.allocate_handle(628, 8);
    u32 h4 = list.allocate_handle(94, 8);

    list.free_handle(h1);
    list.free_handle(h3);
    list.free_handle(h2);
    list.free_handle(h4);
}

void linear_allocator_test() {
    TestCounter counter("Linear Allocator");
    LinearAllocator alloc{500};

    {
        DynamicArray<u8, LinearAllocator> arr(&alloc);
        arr.reserve(200);
        expect(alloc.count() >= 200 * sizeof(u8), counter);

        DynamicArray<u8, LinearAllocator> arr2(&alloc);
        arr2.reserve(200);
        expect(alloc.count() >= 400 * sizeof(u8), counter);

        DynamicArray<u8, LinearAllocator> arr3(&alloc);
        arr3.reserve(300);
        expect(alloc.count() >= 700 * sizeof(u8), counter);
    }
}

void hashmap_test() {
    LinearAllocator alloc(1024 * sizeof(u32));
    HashMap<std::string_view, u32, LinearAllocator, 100> map(&alloc); 
    map.reserve(32);

    std::string_view key1 = "kate_age";
    std::string_view key2 = "paul_age";
    
    map.put(key1, 18u);
    map.put(key2, 20u);

    auto age1 = map.get(key1);
    auto age2 = map.get(key2);
}

void TestManager::collect_all_tests() {
    module_tests.append(hashmap_test);
    module_tests.append(linear_allocator_test);
    module_tests.append(stack_allocator_test);
    module_tests.append(freelist_allocator_test);
    module_tests.append(fixed_array_test);
    module_tests.append(dyn_array_test);
}

} // sf

#endif
