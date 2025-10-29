#pragma once

#ifdef SF_TESTS

#include "sf_core/logger.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_containers/optional.hpp"
#include <string_view>
#include <format>

namespace sf {

struct TestCounter {
    u32 all{0};
    u32 passed{0};
    u32 failed{0};
    std::string_view test_name;

    TestCounter(std::string_view name);
    ~TestCounter();
};

struct TestManager {
public:
    static constexpr u32 MAX_TEST_COUNT{100};
    using TestFn = void(*)();
    FixedArray<TestFn, MAX_TEST_COUNT> module_tests;
public:
    void run_all_tests() const;
    void add_test(TestFn test);
    void collect_all_tests();
};

template<typename ...Args>
bool expect(bool cond, TestCounter& counter, Option<std::format_string<Args...>> fmt = None::VALUE, Args&& ...args) {
    bool not_expected = !cond;

    counter.all++;
    if (not_expected) {
        counter.failed++;
    } else {
        counter.passed++;
    }
    
    if (fmt.is_some()) {
        if (not_expected) {
            LOG_ERROR(fmt.unwrap_copy(), std::forward<Args>(args)...);
            return false;
        } else {
            return true;
        }
    }

    return !not_expected;
}
    
} // sf

#endif
