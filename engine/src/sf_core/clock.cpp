#include "sf_core/clock.hpp"
#include "sf_platform/platform.hpp"

namespace sf {
f64 Clock::update_and_get_delta() {
    if (start_time != 0.0) {
        f64 new_elapsed_time = platform_get_abs_time() - start_time;
        f64 delta_time = new_elapsed_time - elapsed_time;
        elapsed_time = new_elapsed_time;
        return delta_time;
    } else {
        return 0.0;
    }
}

void Clock::start() {
    start_time = platform_get_abs_time();
    elapsed_time = 0.0;
}

void Clock::stop() {
    start_time = 0.0;
}
} // sf
