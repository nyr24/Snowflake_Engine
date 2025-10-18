#pragma once

#include "sf_core/defines.hpp"

namespace sf {

struct Clock {
    f64 start_time;
    f64 elapsed_time;

    f64 update_and_get_delta();
    void start();
    void stop();
    void restart();
    bool is_running();
};

} // sf
