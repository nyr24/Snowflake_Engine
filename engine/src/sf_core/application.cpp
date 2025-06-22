#include "sf_core/application.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/game_types.hpp"

static sf::ApplicationState app_state;
static bool app_initialized{ false };

bool sf::create_app(sf::GameInstance* game_inst) {
    if (app_initialized) {
        LOG_ERROR("application create called more than once");
        return false;
    }

    app_state.game_inst = game_inst;

    // init subsystems
    sf::init_logging();

    app_state.is_running = true;
    app_state.is_suspended = false;
    app_state.platform_state = sf::PlatformState{};

    bool start_success = app_state.platform_state.startup(
        game_inst->app_config.name,
        game_inst->app_config.start_pos_x,
        game_inst->app_config.start_pos_y,
        game_inst->app_config.width,
        game_inst->app_config.height
    );

    if (!start_success) {
        LOG_ERROR("Failed to startup the platform");
        return false;
    }

    if (!game_inst->init(game_inst)) {
        LOG_FATAL("Game failed to initialize");
        return false;
    }

    app_state.game_inst->on_resize(app_state.game_inst, app_state.width, app_state.height);

    app_initialized = true;
    return true;
}

void sf::run_app() {
    while (app_state.is_running) {
        if (!app_state.platform_state.start_event_loop()) {
            app_state.is_running = false;
        }

        if (!app_state.is_suspended) {
            if (!app_state.game_inst->update(app_state.game_inst, 0.0f)) {
                LOG_FATAL("Game update failed, shutting down");
                app_state.is_running = false;
                break;
            }

            if (!app_state.game_inst->render(app_state.game_inst, 0.0f)) {
                LOG_FATAL("Game render failed, shutting down");
                app_state.is_running = false;
                break;
            }
        }
    }

    app_state.is_running = false;
}

