#include "game.hpp"
#include "sf_ds/allocator.hpp"
#include <initializer_list>
#include <sf_ds/optional.hpp>
#include <sf_core/game_types.hpp>
#include <sf_core/entry.hpp>
#include <sf_core/memory_sf.hpp>
#include <sf_core/logger.hpp>
#include <sf_ds/hashmap.hpp>
#include <sf_ds/array_list.hpp>
#include <utility>

struct Simple {
    Simple() = default;

    Simple(i32 val)
        : val{ val }
    {}

    ~Simple() {
        LOG_DEBUG("destroyed, {}", val);
    }

    i32 val;
};

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

    using FixedArr = sf::ArrayList<Simple, sf::FixedBufferAllocator<Simple, 100>>;

    FixedArr s1 = { std::make_pair(
        std::initializer_list<Simple>{ 1, 2, 3, 4, 5, 6 },
        sf::FixedBufferAllocator<Simple, 100>{}
    )};

    LOG_INFO("count: {}, cap: {}", s1.count(), s1.capacity());
    s1.clear();
    LOG_INFO("after: count: {}, cap: {}", s1.count(), s1.capacity());

    // sf::HashMap<FixedArr, int> map{ 100, {
    //     FixedArr::hash
    // }};

    // FixedArr s1 = { std::make_pair(
    //     std::initializer_list<u8>{ 'h', 'e', 'l', 'l', 'o' },
    //     sf::FixedBufferAllocator<u8, 100>{}
    // )};
    //
    // FixedArr  s2 = { std::make_pair(
    //     std::initializer_list<u8>{ 'w', 'e', 'l', 'l', 'o' },
    //     sf::FixedBufferAllocator<u8, 100>{}
    // )};
    //
    // map.put(s1, 1);
    // map.put(s2, 2);
    //
    // auto g1 = map.get(s1);
    // auto g2 = map.get(s2);
    //
    // LOG_INFO("get: {} , {}", g1.unwrap(), g2.unwrap());

    return true;
}
