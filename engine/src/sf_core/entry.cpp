#include "sf_core/entry.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_core/application.hpp"
#include "sf_core/game_types.hpp"
#include "sf_core/logger.hpp"
#include "sf_tests/test_manager.hpp"

i32 main() {
#ifdef SF_TESTS
    sf::TestManager test_manager;
    test_manager.collect_all_tests();
    test_manager.run_all_tests();
#endif
    sf::LinearAllocator game_allocator(sf::get_mem_page_size() * 10);
    sf::GameInstance game_inst{ std::move(game_allocator) };

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
