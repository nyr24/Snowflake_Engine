#pragma once

#include <sf_core/types.hpp>
#include <sf_core/game_types.hpp>

struct GameState {
    f32 delta_time;
};

bool init(const sf_core::GameInstance* game_inst);
bool update(const sf_core::GameInstance* game_inst, f32 delta_time);
bool render(const sf_core::GameInstance* game_inst, f32 delta_time);
void on_resize(const sf_core::GameInstance* game_inst, u32 width, u32 height);
