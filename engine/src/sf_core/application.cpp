#include "sf_core/application.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/game_types.hpp"
#include "sf_containers/optional.hpp"
#include "sf_vulkan/renderer.hpp"

namespace sf {

static ApplicationState application_state;
static bool is_application_initialized{ false };

bool application_create(sf::GameInstance* game_inst) {
    if (is_application_initialized) {
        LOG_ERROR("application create called more than once");
        return false;
    }

    application_state.game_inst = game_inst;

    application_state.config = game_inst->app_config;
    application_state.is_running = true;
    application_state.is_suspended = false;
    application_state.platform_state = sf::PlatformState{};

    event_set_listener(SystemEventCode::APPLICATION_QUIT, nullptr, application_on_event);
    event_set_listener(SystemEventCode::KEY_PRESSED, nullptr, application_on_key);
    event_set_listener(SystemEventCode::KEY_RELEASED, nullptr, application_on_key);
    event_set_listener(SystemEventCode::MOUSE_BUTTON_PRESSED, nullptr, application_on_mouse);
    event_set_listener(SystemEventCode::MOUSE_BUTTON_RELEASED, nullptr, application_on_mouse);

    bool start_success = application_state.platform_state.startup(application_state);

    if (!start_success) {
        LOG_ERROR("Failed to startup the platform");
        return false;
    }

    if (!game_inst->init(game_inst)) {
        LOG_FATAL("Game failed to initialize");
        return false;
    }

    application_state.game_inst->resize(application_state.game_inst, application_state.config.width, application_state.config.height);

    if (!renderer_init(game_inst->app_config, application_state.platform_state)) {
        return false;
    }

    is_application_initialized = true;
    return true;
}

void application_run() {
    application_state.clock.start();

    while (application_state.is_running) {
        if (!application_state.platform_state.poll_events(application_state)) {
            application_state.is_running = false;
        }

        if (!application_state.is_suspended) {
            f64 delta_time = application_state.clock.update_and_get_delta();
            f64 frame_start_time = platform_get_abs_time();

            if (!renderer_begin_frame(delta_time)) {
                LOG_FATAL("Begin frame is failed, shutting down");
                application_state.is_running = false;
                break;  
            }

            if (!application_state.game_inst->update(application_state.game_inst, delta_time)) {
                LOG_FATAL("Game update failed, shutting down");
                application_state.is_running = false;
                break;
            }

            if (!application_state.game_inst->render(application_state.game_inst, delta_time)) {
                LOG_FATAL("Game render failed, shutting down");
                application_state.is_running = false;
                break;
            }

            // TODO: refactor packet creation
            RenderPacket packet;
            packet.delta_time = 0;
            packet.elapsed_time = application_state.clock.elapsed_time;
            renderer_draw_frame(packet);

            f64 frame_elapsed_time = platform_get_abs_time() - frame_start_time;

        #ifdef SF_LIMIT_FRAME_COUNT
            f64 frame_remain_seconds = application_state.TARGET_FRAME_SECONDS - frame_elapsed_time;

            if (frame_remain_seconds > 0.0) {
                u64 remaining_ms = frame_remain_seconds * 1000.0;
                platform_sleep(remaining_ms - 1);
            }
        #endif

            application_state.frame_count++;
            input_update(delta_time);
            renderer_end_frame(delta_time);
        }
    }

    application_state.is_running = false;
}

bool application_on_event(u8 code, void* sender, void* listener_inst, Option<EventContext> context) {
    switch (static_cast<SystemEventCode>(code)) {
        case SystemEventCode::APPLICATION_QUIT: {
            application_state.is_running = false;
            return true;
        } break;
        default: break;
    };

    return false;
}

bool application_on_key(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    if (maybe_context.is_none()) {
        return false;
    }

    EventContext& context{ maybe_context.unwrap() };

    if (code == static_cast<u8>(SystemEventCode::KEY_PRESSED)) {
        u8 key_code = context.data.u8[0];
        switch (static_cast<Key>(key_code)) {
            case Key::ESCAPE: {
                event_execute_callbacks(SystemEventCode::APPLICATION_QUIT, nullptr, {None::VALUE});
                return true;
            };
            default: {
                LOG_DEBUG("Key '{}' was pressed", (char)key_code);
            } break;
        }
    } else if (code == static_cast<u8>(SystemEventCode::KEY_RELEASED)) {
        u8 key_code = context.data.u8[0];
        switch (static_cast<Key>(key_code)) {
            default: {
                LOG_DEBUG("Key '{}' was released", (char)key_code);
            } break;
        }
    }

    return false;
}

bool application_on_mouse(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
    if (maybe_context.is_none()) {
        return false;
    }

    EventContext& context{ maybe_context.unwrap() };

    u8 mouse_btn = context.data.u8[0];
    if (code == static_cast<u8>(SystemEventCode::MOUSE_BUTTON_PRESSED)) {
        switch (static_cast<MouseButton>(mouse_btn)) {
            default: {
                LOG_DEBUG("Mouse btn '{}' was pressed", mouse_btn);
            } break;
        }
    } else if (code == static_cast<u8>(SystemEventCode::MOUSE_BUTTON_RELEASED)) {
        switch (static_cast<MouseButton>(mouse_btn)) {
            default: {
                LOG_DEBUG("Mouse btn '{}' was released", mouse_btn);
            } break;
        }
    }

    return false;
}

} // sf
