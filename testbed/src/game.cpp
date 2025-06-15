#include "game.hpp"
#include <sf_core/logger.hpp>

bool init(const sf::GameInstance* game_inst) {
    LOG_INFO("init() is running!");
    return true;
}

bool update(const sf::GameInstance* game_inst, f32 delta_time) {
    LOG_INFO("update() is running!");
    return true;
}

bool render(const sf::GameInstance* game_inst, f32 delta_time) {
    LOG_INFO("render() is running!");
    return true;
}

void on_resize(const sf::GameInstance* game_inst, u32 width, u32 height) {
}
