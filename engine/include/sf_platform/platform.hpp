#pragma once

#include "sf_core/defines.hpp"
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sf {
struct ApplicationConfig;
struct VulkanContext;

// struct PlatformState {
// public:
//     void* internal_state;

// public:
//     PlatformState();
//     PlatformState(PlatformState&& rhs) noexcept;
//     PlatformState& operator=(PlatformState&& rhs) noexcept;
//     PlatformState(const PlatformState& rhs) = delete;
//     PlatformState& operator=(const PlatformState& rhs) = delete;
//     ~PlatformState();

//     bool startup(ApplicationState& app_state);
//     bool poll_events(ApplicationState& app_state);
// };

struct PlatformState {
public:
    GLFWwindow*            window;
    // TODO: renderer
    // GLFWframebuffersizefun resize_callback;
    GLFWkeyfun             key_callback;
    GLFWcursorposfun       mouse_move_callback;
    GLFWmousebuttonfun     mouse_btn_callback;
    GLFWscrollfun          mouse_wheel_callback;
public:
    static bool create(const ApplicationConfig& config, PlatformState& out_state);
    // void get_required_extensions(FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& out_required_extensions);
    void create_vk_surface(VulkanContext& context);
    void attach_event_callbacks();

    PlatformState() = default;
    ~PlatformState();
};

void*   platform_mem_alloc(u64 byte_size, u16 alignment);
u32     platform_get_mem_page_size();
void    platform_console_write(char* message_buff, u16 written_count, u8 color);
void    platform_console_write_error(char* message_buff, u16 written_count, u8 color);
f64     platform_get_abs_time();
void    platform_sleep(u64 ms);

// void    platform_get_required_extensions(FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions);
// void    platform_create_vk_surface(PlatformState& plat_state, VulkanContext& context);

} // sf
