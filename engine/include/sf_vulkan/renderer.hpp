#pragma once

#include "sf_platform/platform.hpp"
#include "sf_vulkan/types.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace sf {

bool renderer_init(const char* app_name, PlatformState& platform_state);
void renderer_resize(i16 width, i16 height);
bool renderer_begin_frame(f64 delta_time);
bool renderer_end_frame(f64 delta_time);
bool renderer_draw_frame(const RenderPacket& packet);

} // sf
