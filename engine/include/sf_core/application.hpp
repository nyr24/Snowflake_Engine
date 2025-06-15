#pragma once

#include "sf_platform/platform.hpp"
#include "sf_core/types.hpp"

namespace sf {
struct ApplicationConfig {
    const i8*   name;
    i16         start_pos_x;
    i16         start_pos_y;
    i16         width;
    i16         height;
};

struct GameInstance;
struct ApplicationState {
    bool                        is_running;
    bool                        is_suspended;
    i16                         width;
    i16                         height;
    f64                         last_time;
    sf::PlatformState           platform_state;
    GameInstance*               game_inst;
};

bool create_app(GameInstance* game_inst);
void run_app();
}

