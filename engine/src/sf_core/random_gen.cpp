#include "sf_core/defines.hpp"
#include <random>
#include "sf_core/random_gen.hpp"

namespace sf {

static std::random_device rd{};

RandomGenerator::RandomGenerator(i32 min, i32 max)
    : mt{ rd() }
    , distr{ min, max }
{
}

i32 RandomGenerator::gen() {
    return distr(mt);
}

void RandomGenerator::gen_many(std::span<i32> out_numbers) {
    for (u32 i{0}; i < out_numbers.size(); ++i) {
        out_numbers[i] = distr(mt);
    }    
}

} // sf
