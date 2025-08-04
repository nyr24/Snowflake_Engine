#include "sf_core/application.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/game_types.hpp"
#include "sf_platform/defines.hpp"
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

    // init subsystems
    init_logging();

    application_state.width = game_inst->app_config.width;
    application_state.height = game_inst->app_config.height;
    application_state.is_running = true;
    application_state.is_suspended = false;
    application_state.platform_state = sf::PlatformState{};

    event_set_listener(SystemEventCode::APPLICATION_QUIT, nullptr, application_on_event);
    event_set_listener(SystemEventCode::KEY_PRESSED, nullptr, application_on_key);
    event_set_listener(SystemEventCode::KEY_RELEASED, nullptr, application_on_key);
    event_set_listener(SystemEventCode::MOUSE_BUTTON_PRESSED, nullptr, application_on_mouse);
    event_set_listener(SystemEventCode::MOUSE_BUTTON_RELEASED, nullptr, application_on_mouse);

    bool start_success = application_state.platform_state.startup(
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

    application_state.game_inst->resize(application_state.game_inst, application_state.width, application_state.height);

    if (!renderer_init(game_inst->app_config.name, application_state.platform_state)) {
        return false;
    }

    is_application_initialized = true;
    return true;
}

void application_run() {
#ifdef SF_PLATFORM_WAYLAND
    application_state.platform_state.start_event_loop(application_state);
#else
    Clock clock;
    clock.start();

    f64 running_time;
    u8 frame_count;
    constexpr f64 target_frame_seconds = 1.0 / 60.0;

    while (application_state.is_running) {
        if (!application_state.platform_state.start_event_loop()) {
            application_state.is_running = false;
        }

        if (!application_state.is_suspended) {
            f64 delta_time = application_state.clock.update_and_get_delta();
            f64 frame_start_time = platform_get_abs_time();

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
            renderer_draw_frame(packet);

            f64 frame_elapsed_time = platform_get_abs_time() - frame_start_time;
            running_time += frame_elapsed_time;

        #ifdef SF_LIMIT_FRAME_COUNT
            f64 frame_remain_seconds = target_frame_seconds - frame_elapsed_time;

            if (frame_remain_seconds > 0.0) {
                u64 remaining_ms = frame_remain_seconds * 1000.0;
                platform_sleep(remaining_ms - 1);
            }
        #endif

            ++frame_count;

            // update input at the end of the frame
            input_update(delta_time);
        }
    }

    application_state.is_running = false;
#endif
}

bool application_on_event(u8 code, void* sender, void* listener_inst, EventContext* context) {
    switch (static_cast<SystemEventCode>(code)) {
        case SystemEventCode::APPLICATION_QUIT: {
            application_state.is_running = false;
            return true;
        } break;
        default: break;
    };

    return false;
}

bool application_on_key(u8 code, void* sender, void* listener_inst, EventContext* context) {
    if (!context) {
        return false;
    }

    if (code == static_cast<u8>(SystemEventCode::KEY_PRESSED)) {
        u8 key_code = context->data.u8[0];
        switch (static_cast<Key>(key_code)) {
            case Key::ESCAPE: {
                event_execute_callback(static_cast<u8>(SystemEventCode::APPLICATION_QUIT), nullptr, nullptr);
                return true;
            };
            default: {
                LOG_DEBUG("Key '{}' was pressed", (char)key_code);
            } break;
        }
    } else if (code == static_cast<u8>(SystemEventCode::KEY_RELEASED)) {
        u8 key_code = context->data.u8[0];
        switch (static_cast<Key>(key_code)) {
            default: {
                LOG_DEBUG("Key '{}' was released", (char)key_code);
            } break;
        }
    }

    return false;
}

bool application_on_mouse(u8 code, void* sender, void* listener_inst, EventContext* context) {
    if (!context) {
        return false;
    }

    u8 mouse_btn = context->data.u8[0];
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
