#include "game.hpp"
#include "sf_ds/allocator.hpp"
#include <sf_ds/optional.hpp>
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>
#include <sf_core/memory_sf.hpp>
#include <sf_core/logger.hpp>
#include <sf_ds/hashmap.hpp>

bool create_game(sf::GameInstance* out_game) {
    out_game->app_config = sf::ApplicationConfig{
        .name = "SNOWFLAKE_GAME",
        .start_pos_x = 0,
        .start_pos_y = 0,
        .width = 620,
        .height = 780
    };
    out_game->init = init;
    out_game->update = update;
    out_game->render = render;
    out_game->resize = resize;
    out_game->game_state_handle = out_game->allocator.allocate<GameState>();

    return true;
}
