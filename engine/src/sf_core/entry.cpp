#include "sf_core/entry.hpp"
#include "sf_core/game_types.hpp"
#include "sf_core/logger.hpp"

i32 main() {
    sf_core::GameInstance game_inst;
    if (!create_game(&game_inst)) {
        LOG_FATAL("Could not create a game");
        return -1;
    }

    if (!game_inst.init || !game_inst.update || !game_inst.render || !game_inst.on_resize) {
        LOG_FATAL("The game's function pointers must be assigned");
        return -2;
    }

    if (!sf_core::create_app(&game_inst)) {
        LOG_INFO("Application failed to create");
        return 1;
    }

    sf_core::run_app();

    return 0;
}
