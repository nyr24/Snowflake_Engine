#include "sf_core/defines.hpp"
#include <random>
#include "sf_core/random_gen.hpp"

namespace sf {

RandomGenerator::RandomGenerator(i32 min, i32 max)
    : rd{}
    , mt{ rd() }
    , distr{ min, max }
{
}

i32 RandomGenerator::gen() {
    return distr(mt);
}

} // sf
