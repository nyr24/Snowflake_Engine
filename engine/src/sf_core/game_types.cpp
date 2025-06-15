#include "sf_core/game_types.hpp"
#include "sf_platform/platform.hpp"

namespace sf {
GameInstance::~GameInstance() {
    if (game_state) {
        sf::platform_mem_free(game_state, false);
        game_state = nullptr;
    }
}

GameInstance::GameInstance(GameInstance&& rhs) noexcept
    : game_state{ rhs.game_state }
    , init{ rhs.init }
    , update{ rhs.update }
    , render{ rhs.render }
    , on_resize{ rhs.on_resize }
    , app_config{ rhs.app_config }
{
    rhs.game_state = nullptr;
}

GameInstance& GameInstance::operator=(GameInstance&& rhs) noexcept
{
    sf::platform_mem_free(game_state, false);
    game_state = rhs.game_state;
    rhs.game_state = nullptr;
    init = rhs.init;
    update = rhs.update;
    render = rhs.render;
    on_resize = rhs.on_resize;
    app_config = rhs.app_config;
    return *this;
}
} // sf
