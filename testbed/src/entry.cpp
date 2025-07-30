#include "game.hpp"
#include "sf_ds/allocator.hpp"
#include <sf_ds/optional.hpp>
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>
#include <sf_core/memory_sf.hpp>
#include <sf_core/logger.hpp>
#include <sf_ds/hashmap.hpp>
#include <sf_ds/array_list.hpp>

// struct Simple {
//     Simple() = default;
//
//     Simple(i32 val, const char* name)
//         : val{ val }
//         , name{name}
//     {}
//
//     ~Simple() {
//         LOG_DEBUG("destroyed, {}", val);
//     }
//
//     i32 val;
//     const char* name;
// };

bool create_game(sf::GameInstance* out_game) {
    sf::ArenaAllocator arena{};

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
    out_game->game_state = arena.allocate<GameState>();

    return true;
}
