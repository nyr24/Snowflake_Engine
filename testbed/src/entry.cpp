#include "game.hpp"
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>
#include <sf_core/memory_sf.hpp>

bool create_game(sf::GameInstance* out_game) {
    out_game->app_config = sf::ApplicationConfig{
        .name = "CROSSBOW",
        .start_pos_x = 0,
        .start_pos_y = 0,
        .width = 620,
        .height = 780
    };

    out_game->init = init;
    out_game->update = update;
    out_game->render = render;
    out_game->on_resize = on_resize;
    out_game->game_state = sf::sf_mem_alloc(sizeof(GameState), sf::MemoryTag::MEMORY_TAG_UNKNOWN);

    return true;
}
