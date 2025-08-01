#include "sf_platform/defines.hpp"

#if defined(SF_PLATFORM_WINDOWS)
#include "sf_platform/platform.hpp"
#include "sf_core/event.hpp"
#include "sf_core/input.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_vulkan/types.hpp"
#include "sf_ds/array_list.hpp"
#include <Windows.h>
#include <windowsx.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>
#include <cstring>
#include <span>

namespace sf {
struct WindowsInternState {
    HINSTANCE   h_instance;
    HWND        hwnd;
};

struct Rect {
    i32 x;
    i32 y;
    i32 width;
    i32 height;

    Rect(i32 x_in, i32 y_in, i32 width_in, i32 height_in)
        : x{ x_in }, y{ y_in }, width{ width_in }, height{ height_in }
    {
        return Rect{ .x = x, .y = y, .width = width, .height = height };
    }
};

static f64 clock_frequency;
static LARGE_INTEGER start_time;

HRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

PlatformState::PlatformState()
    : internal_state{ platform_mem_alloc(sizeof(WindowsInternState), alignof(WindowsInternState)) }
{
    std::memset(internal_state, 0, sizeof(internal_state));
}

bool PlatformState::startup(
    const char* app_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height
) {
    WindowsInternState* state = static_cast<WindowsInternState*>(this->internal_state);
    state->h_instance = GetModuleHandleA(nullptr);
    HICON icon = LoadIcon(state->h_instance, IDI_APPLICATION);

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLCKS;
    wc.lpfnWndProc = win32_process_message;
    wc.cbClsExtra = 0;
    wc.cbWndEtra = 0;
    wc.hInstance = state->h_instance;
    wc.hIcon = icon;
    wc.cursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = "sf_window_class";

    if (!RegisterClassA(&wc)) {
        MessageBoxA(0, "Window registration failure", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Create a window
    Rect window_client_sizes = Rect(x, y, width, height);
    Rect window_border_sizes = Rect(x, y, width, height);

    u32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
    u32 window_ex_style = WS_EX_APPWINDOW;

    window_style |= WS_MAXIMIZE_BOX;
    window_style |= WS_MINIMIZE_BOX;
    window_style |= WS_THICHFRAME;

    RECT border_rect = { 0, 0, 0, 0 };
    AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

    window_border_sizes.x += border_rect.left;
    window_border_sizes.y += border_rect.top;
    window_border_sizes.width = border_rect.right - border_rect.left;
    window_border_sizes.height = border_rect.bottom - border_rect.top;

    HWND handle = CreateWindowExA(
        window_ex_style, "sf_window_class", app_name,
        window_style, window_border_sizes.x, window_border_sizes.y, window_border_sizes.width, window_border_sizes.height,
        0, 0, state->h_instance, 0
    );

    if (handle == nullptr) {
        MessageBoxA(nullptr, "Window creation failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        LOG_FATAL("Window creation failed!");
        return false;
    }
    state->hwnd = handle;

    bool should_activate = true;
    i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
    ShowWindow(state->hwnd, show_window_command_flags);

    // Clock setup
    LARGE_INTERGER frequency;
    QueryPerformanceFrequency(&frequency);
    clock_frequency = 1.0 / (f64)frequency.QuadPart;
    QueryPerformanceCounter(&start_time);

    return true;
}

PlatformState::~PlatformState() {
    WindowsInternState* intern_state = static_cast<WindowsInternState>(this->internal_state);
    if (intern_state->hwnd) {
        DestroyWindow(intern_state->hwnd);
        intern_state->hwnd = nullptr;
        sf_mem_free(intern_state, sizeof(WindowsInternState), alignof(WindowsInternState));
    }
}

bool PlatformState::start_event_loop(ApplicationState& app_state) {
    MSG message;
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return true;
}

f64 platform_get_abs_time() {
    LARGE_INTERGER now_time;
    QueryPerformanceCounter(&now_time);
    return (f64)now_time.QuadPart * clock_frequency;
}

void* platform_mem_alloc(u64 byte_size, u16 alignment = 0) {
    HANDLE heap_handle = GetProcessHeap();
    void* ptr = HeapAlloc(heap_handle, HEAP_ZERO_MEMORY, byte_size);
    if (!ptr) {
        panic("Out of memory");
    }
    return ptr;
}

void platform_console_write(const i8* message, u8 color) {
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    static u8 levels[6] = { 64, 4, 6, 2, 1, 8 };
    SetConsoleTextAttribute(console_handle, levels[color]);

    OutputDebugStringA(message.data());
    LPDWORD count_written = 0;
    WriteConsoleA(console_handle, message, (DWORD)message.length(), count_written, 0);
}

void platform_console_write_error(const i8* message, u8 color) {
    HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
    static u8 levels[6] = { 64, 4, 6, 2, 1, 8 };
    SetConsoleTextAttribute(console_handle, levels[color]);

    OutputDebugStringA(message.data());
    LPDWORD count_written = 0;
    WriteConsoleA(console_handle, message, (DWORD)message.length(), count_written, 0);
}

void platform_sleep(u64 ms) {
    Sleep(ms);
}

u32 platform_get_mem_page_size() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<u32>(si.dwPageSize);
}

void platform_get_required_extensions(FixedArrayList<const char*, 5>& required_extensions) {
    required_extensions.append("VK_KHR_win32_surface");
}

void platform_create_vk_surface(PlatformState& plat_state, VulkanContext& context) {
    WindowsInternState* state = static_cast<WindowsInternState*>(plat_state.internal_state);

    VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    create_info.hinstance = state->h_instance;
    create_info.hwnd = state->hwnd;

    sf_vk_check(vkCreateWin32SurfaceKHR(context.instance, &create_info, &context.allocator, &context.surface));
}

HRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_ERASEBKGND:
            // will be handled by the application
            return 1;
        case WM_CLOSE:
            // will be handled by the application
            event_fire(static_cast<u8>(SystemEventCode::APPLICATION_QUIT), nullptr, nullptr);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            // Get the updated size
            RECT r;
            GetClientRect(hwnd, &r);
            u32 width = r.right - r.left;
            u32 height = r.bottom - r.top;

            EventContext context;
            context.data.u32[0] = width;
            context.data.u32[1] = height;

            event_fire(static_cast<u8>(SystemEventCode::RESIZED), nullptr, &context);
        } break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN;
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            bool is_pressed = (msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN);
            u8 key = static_cast<u8>(w_param);
            input_process_key(key, is_pressed);
        } break;
        case WM_MOUSEMOVE: {
            i16 x_pos = static_cast<i16>(GET_X_PARAM(l_param));
            i16 y_pos = static_cast<i16>(GET_Y_PARAM(l_param));
            input_process_mouse_move(MousePos{ .x = x_pos, .y = y_pos });
        } break;
        case MOUSEWHEEL: {
            i32 z_delta = GET_WHEEL_DELTA_PARAM(w_param);
            if (z_delta != 0) {
                z_delta = z_delta < 0 ? -1 : 1;
            }
            input_process_mouse_wheel(static_cast<i8>(z_delta));
        } break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            bool is_pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            MouseButton button = MouseButton::LEFT;
            switch (msg) {
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP: {
                    button = MouseButton::RIGHT;
                } break;
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP: {
                    button = MouseButton::MIDDLE;
                } break;
                default: break;
            };

            input_process_button(button, is_pressed);
        } break;
        default: return DefWindowProc(hwnd, msg, w_param, l_param);
    }
}

#endif // SF_PLATFORM_WINDOWS
