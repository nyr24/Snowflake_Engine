#pragma once

#include "sf_core/defines.hpp"
#include <random>
#include <span>

namespace sf {

struct RandomGenerator {
    std::mt19937 mt;
    std::uniform_int_distribution<i32> distr;

    RandomGenerator(i32 min, i32 max);
    i32 gen();
    void gen_many(std::span<i32> out_numbers);
};

} // sf
