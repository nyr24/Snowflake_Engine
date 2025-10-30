#include "sf_platform/platform.hpp"
#include "sf_core/application.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/input.hpp"
#include "sf_core/logger.hpp"
#include <cstdlib>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "GLFW/glfw3.h"

namespace sf {

static void platform_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mode);
static void platform_mouse_move_callback(GLFWwindow* window, f64 x, f64 y);
static void platform_mouse_btn_callback(GLFWwindow* window, i32 btn, i32 action, i32 mods);
static void platform_mouse_wheel_callback(GLFWwindow* window, f64 xoffset, f64 yoffset);

bool PlatformState::create(const ApplicationConfig& config, PlatformState& out_state)
{
    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    out_state.window = glfwCreateWindow(config.window_width, config.window_height, config.name, nullptr, nullptr);
    if (!out_state.window) {
        return false;
    }

    glfwMakeContextCurrent(out_state.window);
    // glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

    out_state.attach_event_callbacks();

    SF_ASSERT_MSG(out_state.key_callback && out_state.mouse_move_callback && out_state.mouse_btn_callback
        && /* out_state.resize_callback && */ out_state.mouse_wheel_callback, "Callbacks should be set");

    // glfwSetFramebufferSizeCallback(out_state.window, out_state.resize_callback);
    glfwSetKeyCallback(out_state.window, out_state.key_callback);
    glfwSetMouseButtonCallback(out_state.window, out_state.mouse_btn_callback);
    glfwSetCursorPosCallback(out_state.window, out_state.mouse_move_callback);
    glfwSetScrollCallback(out_state.window, out_state.mouse_wheel_callback);
    glfwSetInputMode(out_state.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return true;
}

void PlatformState::attach_event_callbacks() {
    key_callback = &platform_key_callback;
    mouse_move_callback = &platform_mouse_move_callback;
    mouse_btn_callback = &platform_mouse_btn_callback;
    mouse_wheel_callback = &platform_mouse_wheel_callback;
}

// void platform_set_glfw_user_state(GLFWwindow* window, GameState *game_state) {
//     glfwSetWindowUserPointer(window, game_state);
// }

// static void platform_resize_callback()
// {
    
// }

static void platform_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mode)
{
    LOG_INFO("key: {}, code: {}", key, scancode);
    input_process_key(static_cast<Key>(key), action == GLFW_PRESS);
}

static void platform_mouse_move_callback(GLFWwindow* window, f64 x, f64 y)
{
    input_process_mouse_move(MousePos{ static_cast<f32>(x), static_cast<f32>(y) });
}

static void platform_mouse_btn_callback(GLFWwindow* window, i32 btn, i32 action, i32 mods)
{
    input_process_mouse_button(static_cast<MouseButton>(btn), action == GLFW_PRESS);
}

static void platform_mouse_wheel_callback(GLFWwindow* window, f64 xoffset, f64 yoffset)
{
    if (std::abs(yoffset) > std::numeric_limits<f64>::epsilon()) {
        input_process_mouse_wheel(yoffset > 0 ? 1 : -1);
    }
}

// void PlatformState::get_required_extensions(FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& out_required_extensions) {
//     out_required_extensions.append(VK_KHR_SURFACE_EXTENSION_NAME);
// }

PlatformState::~PlatformState()
{
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
    }
}

} // sf
