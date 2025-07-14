#include "sf_core/entry.hpp"
#include "sf_core/application.hpp"
#include "sf_core/game_types.hpp"
#include "sf_core/logger.hpp"

i32 main() {
    sf::GameInstance game_inst;
    if (!create_game(&game_inst)) {
        LOG_FATAL("Could not create a game");
        return -1;
    }

    if (!game_inst.init || !game_inst.update || !game_inst.render || !game_inst.on_resize) {
        LOG_FATAL("The game's function pointers must be assigned");
        return -2;
    }

    if (!sf::application_create(&game_inst)) {
        LOG_INFO("Application failed to create");
        return 1;
    }

    sf::application_run();

    return 0;
}
