#pragma once

#include "sf_core/event.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/types.hpp"
#include <chrono>

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
    using TimepointType = std::chrono::time_point<std::chrono::steady_clock>;

    bool                        is_running;
    bool                        is_suspended;
    i16                         width;
    i16                         height;
    TimepointType               last_time;
    sf::PlatformState           platform_state;
    GameInstance*               game_inst;
};

bool application_create(GameInstance* game_inst);
void application_run();
bool application_on_event(u8 code, void* sender, void* listener_inst, EventContext* context);
bool application_on_key(u8 code, void* sender, void* listener_inst, EventContext* context);

}

