#pragma once

#include "sf_core/application.hpp"
#include "sf_allocators/linear_allocator.hpp"

namespace sf {
struct GameInstance {
    using InitFn = bool(*)(const GameInstance*);
    using UpdateFn = bool(*)(const GameInstance*, f64 delta_time);
    using RenderFn = bool(*)(const GameInstance*, f64 delta_time);
    using ResizeFn = void(*)(const GameInstance*, u32 width, u32 height);

    GameInstance(LinearAllocator& allocator)
        : allocator{ allocator }
    {}

    InitFn init;
    UpdateFn update;
    RenderFn render;
    ResizeFn resize;
    LinearAllocator& allocator;
    ApplicationConfig app_config;
    u32 game_state_handle;
};
}
