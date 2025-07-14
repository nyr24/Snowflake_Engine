#pragma once

#include "sf_core/application.hpp"
#include <functional>

namespace sf {
struct GameInstance {
    GameInstance() = default;
    ~GameInstance();
    GameInstance(GameInstance&& rhs) noexcept;
    GameInstance& operator=(GameInstance&& rhs) noexcept;
    GameInstance(const GameInstance& rhs) = delete;
    GameInstance& operator=(const GameInstance& rhs) = delete;

    std::function<bool(const GameInstance*)>                        init;
    std::function<bool(const GameInstance*, f64 delta_time)>        update;
    std::function<bool(const GameInstance*, f64 delta_time)>        render;
    std::function<void(const GameInstance*, u32 width, u32 height)> on_resize;
    void*                                                           game_state;
    sf::ApplicationConfig                                           app_config;
};
}
