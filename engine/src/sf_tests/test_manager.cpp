#ifdef SF_TESTS

#include "sf_core/io.hpp"
#include "sf_containers/bitset.hpp"
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
    LOG_TEST("Tests for {} started: ", name);
}

TestCounter::~TestCounter() {
    LOG_TEST("Tests for {} ended:\t\n{} all, passed: {}, failed: {}", test_name, all, passed, failed);
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

    constexpr usize BIG_SIZE = 1'000'000;

    {
        LinearAllocator al;
        DynamicArray<u8, LinearAllocator, true> arr(32, &al);
        clock.restart();
        for (usize i{0}; i < BIG_SIZE; ++i) {
            arr.append(rand());
        }
        auto time = clock.update_and_get_delta();
        LOG_TEST("new dyn array (handle): {}", time);
    }

    {
        GeneralPurposeAllocator al;
        DynamicArray<u8, GeneralPurposeAllocator, false> arr(32, &al);
        clock.restart();
        for (usize i{0}; i < BIG_SIZE; ++i) {
            arr.append(rand());
        }
        auto time = clock.update_and_get_delta();
        LOG_TEST("new dyn array (no handle): {}", time);
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
    FreeList alloc(512);

    DynamicArray<u8, FreeList<true>> arr(500, &alloc);

    // lookup data
    for (usize i{0}; i < 5; ++i) {
        arr.append(i);
    }

    // trigger resize on first array
    arr.resize(600);

    // log lookup data
    LOG_TEST("lookup data after resize: ");
    for (usize i{0}; i < 5; ++i) {
        LOG_TEST("{} ", arr[i]);
    }
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
    TestCounter counter("HashMap");
    
    LinearAllocator alloc(1024 * sizeof(usize));
    HashMap<std::string_view, usize, LinearAllocator> map(&alloc); 
    map.reserve(32);

    std::string_view key1 = "kate_age";
    std::string_view key2 = "paul_age";
    
    map.put(key1, 18ul);
    map.put(key2, 20ul);

    auto age1 = map.get(key1);
    auto age2 = map.get(key2);

    expect(age1.is_some(), counter);
    expect(age2.is_some(), counter);

    auto del1 = map.remove(key1);
    auto del2 = map.remove(key1);
    auto del3 = map.remove(key2);

    expect(del1, counter);
    expect(!del2, counter);
    expect(del3, counter);
}

void bitset_test() {
    TestCounter counter{"BitSet"};
    BitSet<256> bitset{};

    bitset.set_bit(2);
    bitset.set_bit(18);
    bitset.set_bit(34);
    bitset.set_bit(56);
    bitset.set_bit(112);
    bitset.set_bit(213);

    expect(bitset.is_bit(2), counter);
    expect(bitset.is_bit(18), counter);
    expect(bitset.is_bit(34), counter);
    expect(bitset.is_bit(56), counter);
    expect(bitset.is_bit(112), counter);
    expect(bitset.is_bit(213), counter);
    expect(bitset.is_bit(118) == false, counter);
    expect(bitset.is_bit(35) == false, counter);
    expect(bitset.is_bit(218) == false, counter);
    expect(bitset.is_bit(59) == false, counter);

    bitset.unset_bit(2);
    bitset.unset_bit(18);
    bitset.unset_bit(34);
    bitset.unset_bit(56);
    bitset.unset_bit(112);
    bitset.unset_bit(213);

    expect(bitset.is_bit(2) == false, counter);
    expect(bitset.is_bit(18) == false, counter);
    expect(bitset.is_bit(34) == false, counter);
    expect(bitset.is_bit(56) == false, counter);
    expect(bitset.is_bit(112) == false, counter);
    expect(bitset.is_bit(213) == false, counter);

    bitset.toggle_bit(2);
    bitset.toggle_bit(18);
    bitset.toggle_bit(34);
    bitset.toggle_bit(56);
    bitset.toggle_bit(112);
    bitset.toggle_bit(213);

    expect(bitset.is_bit(2), counter);
    expect(bitset.is_bit(18), counter);
    expect(bitset.is_bit(34), counter);
    expect(bitset.is_bit(56), counter);
    expect(bitset.is_bit(112), counter);
    expect(bitset.is_bit(213), counter);
}

void filesystem_test() {
    TestCounter counter{"filesystem"};

    {
        FixedArray<std::string_view, 4> input{ "/location/number/one.jpg", "one.jpg", "locationone", "garbage@/value.png" };
        FixedArray<std::string_view, 4> expected_output{ "one", "one", "locationone", "value" };

        for (u32 i{0}; i < input.count(); ++i) {
            auto res = trim_dir_and_extension_from_path(input[i]);
            expect(res == expected_output[i], counter, {"Filesystem test expected: equal to {:s}, found {:s}"}, expected_output[i], res);
        }
    }

    {
        FixedArray<std::string_view, 3> input{ "/location/number/one.jpg", "path1/location/two/one.png", "locationone/locationtwo/" };
        FixedArray<std::string_view, 3> expected_output{"/location/number/", "path1/location/two/", "locationone/locationtwo/" };

        for (u32 i{0}; i < input.count(); ++i) {
            auto res = strip_file_name_from_path(input[i]);
            expect(res == expected_output[i], counter, {"Filesystem test expected: equal to {:s}, found {:s}"}, expected_output[i], res);
        }
    }

    {
        FixedArray<std::string_view, 4> input{ "/location/number/one.jpg", "path1/location/two.png", "input1/here.imdb", "here" };
        FixedArray<std::string_view, 4> strip{ "/location/num", "path1/location/", "input2/here", "input4here" };
        FixedArray<std::string_view, 4> expected_output{"ber/one", "two", "1/here", "here" };

        for (u32 i{0}; i < input.count(); ++i) {
            auto res = strip_part_from_start_and_extension(input[i], strip[i]);
            expect(res == expected_output[i], counter, {"Filesystem test expected: equal to {:s}, found {:s}"}, expected_output[i], res);
        }
    }
}

void TestManager::collect_all_tests() {
    module_tests.append(hashmap_test);
    module_tests.append(linear_allocator_test);
    module_tests.append(stack_allocator_test);
    module_tests.append(freelist_allocator_test);
    module_tests.append(fixed_array_test);
    module_tests.append(dyn_array_test);
    module_tests.append(bitset_test);
    module_tests.append(filesystem_test);
}

} // sf

#endif
