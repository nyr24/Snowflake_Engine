#pragma once

#include "sf_core/application.hpp"
#include "sf_allocators/linear_allocator.hpp"
#include "sf_core/constants.hpp"

namespace sf {
struct GameInstance {
public:
    using InitFn   = bool(*)(const GameInstance*);
    using UpdateFn = bool(*)(const GameInstance*, f64 delta_time);
    using RenderFn = bool(*)(const GameInstance*, f64 delta_time);
    using ResizeFn = void(*)(const GameInstance*, u32 width, u32 height);

public:
    InitFn init;
    UpdateFn update;
    RenderFn render;
    ResizeFn resize;
    LinearAllocator   allocator;
    ApplicationConfig app_config;
    u32 game_state_handle;

public:
    GameInstance(LinearAllocator&& allocator)
        : allocator{ std::move(allocator) }
    {}

    ~GameInstance()
    {
        if (game_state_handle != INVALID_ALLOC_HANDLE) {
            allocator.free_handle(game_state_handle);
            game_state_handle = INVALID_ALLOC_HANDLE;
        }
    }
};
}
