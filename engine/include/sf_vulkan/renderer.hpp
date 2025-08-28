#pragma once

#include "sf_core/application.hpp"
#include "sf_platform/platform.hpp"
#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace sf {

bool renderer_init(ApplicationConfig& config, PlatformState& platform_state);
void renderer_resize(u16 width, u16 height);
bool renderer_begin_frame(f64 delta_time);
void renderer_end_frame(f64 delta_time);
bool renderer_draw_frame(const RenderPacket& packet);

} // sf
