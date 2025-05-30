#include "sf_core/application.hpp"
#include "sf_core/logger.hpp"

static sf_core::ApplicationState app_state;
static bool app_initialized{ false };

bool sf_core::create_app(const sf_core::ApplicationConfig& config) {
    if (app_initialized) {
        LOG_ERROR("application create called more than once");
        return false;
    }

    // init subsystems
    sf_core::init_logging();

    app_state.is_running = true;
    app_state.is_suspended = false;
    app_state.platform_state = sf_platform::PlatformState{};

    bool start_success = app_state.platform_state.startup(
        config.name,
        config.start_pos_x,
        config.start_pos_y,
        config.width,
        config.height
    );

    if (!start_success) {
        LOG_ERROR("Failed to startup the platform");
        return false;
    }

    LOG_DEBUG("This is warning! and n: {}", 12);

    app_initialized = true;
    return true;
}

void sf_core::run_app() {
    while (app_state.is_running) {
        if (!app_state.platform_state.start_event_loop()) {
            app_state.is_running = false;
        }
    }

    app_state.is_running = false;
}

