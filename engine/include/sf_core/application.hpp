#pragma once

#include "sf_core/defines.hpp"
#include "sf_core/clock.hpp"
#include "sf_core/event.hpp"
#include "sf_platform/platform.hpp"

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
    sf::PlatformState           platform_state;
    GameInstance*               game_inst;
    Clock                       clock;
    i16                         width;
    i16                         height;
    bool                        is_running;
    bool                        is_suspended;
};

bool application_create(GameInstance* game_inst);
void application_run();
bool application_on_event(u8 code, void* sender, void* listener_inst, EventContext* context);
bool application_on_mouse(u8 code, void* sender, void* listener_inst, EventContext* context);
bool application_on_key(u8 code, void* sender, void* listener_inst, EventContext* context);

}

