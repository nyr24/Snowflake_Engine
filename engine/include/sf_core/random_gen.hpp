#pragma once

#include "sf_core/defines.hpp"
#include <random>

namespace sf {

struct RandomGenerator {
    std::random_device   rd;
    std::mt19937         mt;
    std::uniform_int_distribution<i32> distr;

    RandomGenerator(i32 min, i32 max);
    i32 gen();
};

} // sf
