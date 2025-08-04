#include "sf_core/entry.hpp"
#include "sf_core/application.hpp"
#include "sf_core/game_types.hpp"
#include "sf_core/logger.hpp"
#include "sf_ds/allocator.hpp"

i32 main() {
    sf::ArenaAllocator global_object_allocator(sf::platform_get_mem_page_size() * 10);
    sf::GameInstance game_inst{ global_object_allocator };

    if (!create_game(&game_inst)) {
        LOG_FATAL("Could not create a game");
        return -1;
    }

    if (!game_inst.init || !game_inst.update || !game_inst.render || !game_inst.resize) {
        LOG_FATAL("The game's function pointers must be assigned");
        return -2;
    }

    if (!sf::application_create(&game_inst)) {
        LOG_FATAL("Application failed to create");
        return 1;
    }

    sf::application_run();

    return 0;
}
