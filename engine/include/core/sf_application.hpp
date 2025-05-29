#pragma once

#include "platform/sf_platform.hpp"
#include "core/sf_types.hpp"

namespace sf_core {
struct ApplicationConfig {
    const i8*   name;
    i16         start_pos_x;
    i16         start_pos_y;
    i16         width;
    i16         height;
};

struct ApplicationState {
    bool                        is_running;
    bool                        is_suspended;
    i16                         width;
    i16                         height;
    sf_platform::PlatformState  platform_state;
    f64                         last_time;
};

bool create_app(const sf_core::ApplicationConfig& config);
void run_app();
}

