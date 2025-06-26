#include "game.hpp"
#include "sf_ds/iterator.hpp"
#include "sf_ds/traits.hpp"
#include <initializer_list>
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>
#include <sf_core/memory_sf.hpp>
#include <sf_core/logger.hpp>
#include <sf_ds/array_list.hpp>
#include <sf_ds/llist.hpp>
#include <utility>

// template<typename T>
// using MyIter = sf::PtrRandomAccessIterator<T>;
//
// template<typename T, sf::IteratorTrait<T>>
struct Simple {
    Simple(u64 d, bool is_dead)
        : damage{ d }, is_dead{ is_dead }
    {}

    u64 damage;
    bool is_dead;
};

bool create_game(sf::GameInstance* out_game) {
    // sf::ArenaAllocator arena{};

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
    // out_game->game_state = arena.allocate<GameState>();

    sf::LinkedArrayList<int> llist {
        std::make_pair(std::initializer_list{ 1, 2, 3, 4, 5, 6 }, 6)
    };

    llist.remove_at(0);
    llist.remove_at(1);
    llist.remove_at(2);
    llist.remove_at(4);
    llist.remove_at(5);
    llist.insert(666);
    llist.insert(777);
    llist.insert(888);
    llist.insert(999);
    llist.insert(1111);
    llist.insert(2222);

    for (auto it = llist.begin(); it != llist.end(); ++it) {
        LOG_INFO("{} ", it->value);
    }

    return true;
}
