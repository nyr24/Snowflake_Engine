#include "sf_core/application.hpp"
#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/game_types.hpp"
#include "sf_containers/optional.hpp"
#include "sf_platform/platform.hpp"
#include "sf_vulkan/device.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/texture.hpp"
#include "sf_platform/glfw3.h"

namespace sf {

static const u32 TEMP_ALLOCATOR_INIT_PAGES{ 16 };

static ApplicationState state;
static GeneralPurposeAllocator gpa;

ApplicationState::ApplicationState()
    : system_allocator{ TextureSystemState::get_memory_requirement() + MaterialSystemState::get_memory_requirement() + EventSystemState::get_memory_requirement() }
    , temp_allocator{ platform_get_mem_page_size() * TEMP_ALLOCATOR_INIT_PAGES }
    , platform_state{}
{}

void application_init_internal_state(const VulkanDevice& device) {
    EventSystemState::create(state.system_allocator, state.event_system);
    TextureSystemState::create(state.system_allocator, device, state.texture_system);
    MaterialSystemState::create(state.system_allocator, state.material_system);

    event_system_init_internal_state(&state.event_system);
    texture_system_init_internal_state(&state.texture_system);
    material_system_init_internal_state(&state.material_system);
}

bool application_create(GameInstance* game_inst) {
    state.game_inst = game_inst;
    state.config = game_inst->app_config;
    state.is_running = true;
    state.is_suspended = false;

    bool platform_init_success = PlatformState::create(game_inst->app_config, state.platform_state);

    if (!platform_init_success) {
        LOG_ERROR("Failed to startup the platform");
        return false;
    }

    if (!game_inst->init(game_inst)) {
        LOG_FATAL("Game failed to initialize");
        return false;
    }

    state.game_inst->resize(state.game_inst, state.config.window_width, state.config.window_height);

    VulkanDevice* selected_device;
    if (!renderer_init(game_inst->app_config, state.platform_state, selected_device)) {
        LOG_FATAL("Renderer failed to initialize");
        return false;
    }
    if (!selected_device) {
        LOG_FATAL("Appropriate device was not found");
        return false;
    }

    application_init_internal_state(*selected_device);

    if (!renderer_post_init()) {
        LOG_FATAL("Renderer failed to post-initialize");
        return false;
    }

    event_system_add_listener(SystemEventCode::APPLICATION_QUIT, nullptr, application_on_event);
    event_system_add_listener(SystemEventCode::KEY_PRESSED, nullptr, application_on_key);
    // event_system_add_listener(SystemEventCode::KEY_RELEASED, nullptr, application_on_key);
    // event_system_add_listener(SystemEventCode::MOUSE_BUTTON_PRESSED, nullptr, application_on_mouse);
    // event_system_add_listener(SystemEventCode::MOUSE_BUTTON_RELEASED, nullptr, application_on_mouse);

    return true;
}

void application_run() {
    state.clock.start();

    while (!glfwWindowShouldClose(state.platform_state.window) && state.is_running) {
        glfwPollEvents();

        if (!state.is_suspended) {
            f64 delta_time = state.clock.update_and_get_delta();
        #ifdef SF_LIMIT_FRAME_COUNT
            f64 frame_start_time = platform_get_abs_time();
        #endif

            if (!renderer_begin_frame(delta_time)) {
                LOG_INFO("Frame is skipped");
                continue;
            }

            if (!state.game_inst->update(state.game_inst, delta_time)) {
                LOG_FATAL("Game update failed, shutting down");
                state.is_running = false;
                break;
            }

            if (!state.game_inst->render(state.game_inst, delta_time)) {
                LOG_FATAL("Game render failed, shutting down");
                state.is_running = false;
                break;
            }

            // TODO: refactor packet creation
            RenderPacket packet;
            packet.delta_time = 0;
            packet.elapsed_time = state.clock.elapsed_time;
            renderer_draw_frame(packet);

        #ifdef SF_LIMIT_FRAME_COUNT
            f64 frame_elapsed_time = platform_get_abs_time() - frame_start_time;
            f64 frame_remain_seconds = state.TARGET_FRAME_SECONDS - frame_elapsed_time;

            if (frame_remain_seconds > 0.0) {
                u64 remaining_ms = frame_remain_seconds * 1000.0;
                platform_sleep(remaining_ms - 1);
            }
        #endif

            state.frame_count++;
            input_update(delta_time);
            renderer_end_frame(delta_time);
        }
    }

    state.is_running = false;
}

bool application_on_event(u8 code, void* sender, void* listener_inst, Option<EventContext> context) {
    switch (static_cast<SystemEventCode>(code)) {
        case SystemEventCode::APPLICATION_QUIT: {
            state.is_running = false;
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
        u16 key_code = context.data.u16[0];
        switch (key_code) {
            case GLFW_KEY_ESCAPE: {
                event_system_fire_event(SystemEventCode::APPLICATION_QUIT, nullptr, {None::VALUE});
                return true;
            };
            default: {
                // LOG_DEBUG("Key '{}' was pressed", (char)key_code);
            } break;
        }
    }
    // else if (code == static_cast<u8>(SystemEventCode::KEY_RELEASED)) {
    //     u16 key_code = context.data.u16[0];
    //     switch (key_code) {
    //         default: {
    //             LOG_DEBUG("Key '{}' was released", (char)key_code);
    //         } break;
    //     }
    // }

    return false;
}

// bool application_on_mouse(u8 code, void* sender, void* listener_inst, Option<EventContext> maybe_context) {
//     if (maybe_context.is_none()) {
//         return false;
//     }

//     EventContext& context{ maybe_context.unwrap() };

    // u8 mouse_btn = context.data.u8[0];
    // if (code == static_cast<u8>(SystemEventCode::MOUSE_BUTTON_PRESSED)) {
    //     switch (static_cast<MouseButton>(mouse_btn)) {
    //         default: {
    //             LOG_DEBUG("Mouse btn '{}' was pressed", mouse_btn);
    //         } break;
    //     }
    // } else if (code == static_cast<u8>(SystemEventCode::MOUSE_BUTTON_RELEASED)) {
    //     switch (static_cast<MouseButton>(mouse_btn)) {
    //         default: {
    //             LOG_DEBUG("Mouse btn '{}' was released", mouse_btn);
    //         } break;
    //     }
    // }

//     return false;
// }

StackAllocator& application_get_temp_allocator() {
    return state.temp_allocator;
}

GeneralPurposeAllocator& application_get_gpa() {
    return gpa;
}

} // sf
