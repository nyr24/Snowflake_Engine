#pragma once

#include "sf_platform/platform.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace sf {
struct RenderPacket {
    f64 delta_time;
};

struct VulkanRenderer {
    PlatformState*  platform_state;
    u64             frame_count;

public:
    VulkanRenderer() = default;
};

struct VulkanContext {
    VkInstance                  instance;
    VkAllocationCallbacks       allocator;
#ifdef SF_DEBUG
    VkDebugUtilsMessengerEXT    debug_messenger;
#endif

public:
    ~VulkanContext();
};

bool renderer_init(const char* app_name, PlatformState* platform_state);
void renderer_resize(i16 width, i16 height);
bool renderer_begin_frame(f64 delta_time);
bool renderer_end_frame(f64 delta_time);
bool renderer_draw_frame(const RenderPacket& packet);
void sf_vk_check(VkResult vk_result);
u32 sf_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData
);

} // sf
