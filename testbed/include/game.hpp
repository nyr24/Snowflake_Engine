#pragma once

#include <sf_core/defines.hpp>
#include <sf_core/game_types.hpp>

struct GameState {
    GameState() = default;
    GameState(f64 delta_time_in)
        : delta_time{ delta_time_in }
    {}

    f64 delta_time;
};

bool init(const sf::GameInstance* game_inst);
bool update(const sf::GameInstance* game_inst, f64 delta_time);
bool render(const sf::GameInstance* game_inst, f64 delta_time);
void resize(const sf::GameInstance* game_inst, u32 width, u32 height);
