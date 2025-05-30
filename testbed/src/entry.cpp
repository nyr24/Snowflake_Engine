#include "game.hpp"
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>

// TODO: remove platform code
#include <sf_platform/platform.hpp>

bool create_game(sf_core::GameInstance* out_game) {
    out_game->app_config = sf_core::ApplicationConfig{
        .name = "SNOWFLAKE",
        .start_pos_x = 0,
        .start_pos_y = 0,
        .width = 620,
        .height = 780
    };

    out_game->init = init;
    out_game->update = update;
    out_game->render = render;
    out_game->on_resize = on_resize;
    out_game->game_state = sf_platform::platform_alloc<GameState>(1, false);

    return true;
}
