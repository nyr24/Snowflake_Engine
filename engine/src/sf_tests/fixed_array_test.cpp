#include "sf_containers/fixed_array.hpp"

namespace sf {

template<typename T>
bool expect(T actual, T expected) {
    return actual == expected;
}

FixedArray<const char*, 10> arr{ "hello", "world", "crazy", "boy" };
const auto res = expect(arr[0], "hello");

} // sf
