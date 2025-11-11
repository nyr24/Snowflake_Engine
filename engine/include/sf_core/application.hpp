#pragma once

#include "sf_allocators/arena_allocator.hpp"
#include "sf_allocators/stack_allocator.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/clock.hpp"
#include "sf_core/event.hpp"
#include "sf_platform/platform.hpp"
#include "sf_vulkan/material.hpp"
#include "sf_vulkan/mesh.hpp"
#include "sf_vulkan/texture.hpp"

namespace sf {
struct ApplicationConfig {
    const char* name;
    u16         window_x;
    u16         window_y;
    u16         window_width;
    u16         window_height;
};

struct GameInstance;

struct ApplicationState {
public:
    static constexpr f64 TARGET_FRAME_SECONDS = 1.0 / 60.0;
    ArenaAllocator              main_allocator;
    StackAllocator              temp_allocator;
    EventSystemState            event_system;
    TextureSystemState          texture_system;
    MaterialSystemState         material_system;
    MeshSystemState             mesh_system;
    PlatformState               platform_state;
    GameInstance*               game_inst;
    Clock                       clock;
    u32                         frame_count;
    ApplicationConfig           config;
    bool                        is_running;
    bool                        is_suspended;

    ApplicationState();
};

bool application_create(GameInstance* game_inst);
void application_run();
bool application_on_event(u8 code, void* sender, void* listener_inst, Option<EventContext> context);
bool application_on_mouse(u8 code, void* sender, void* listener_inst, Option<EventContext> context);
bool application_on_key(u8 code, void* sender, void* listener_inst, Option<EventContext> context);
ArenaAllocator& application_get_main_allocator();
StackAllocator& application_get_temp_allocator();

}

