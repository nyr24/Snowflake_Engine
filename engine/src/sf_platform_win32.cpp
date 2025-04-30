#include <cstring>
#include "sf_util.hpp"
#include "sf_platform.hpp"
#include "logger.hpp"

namespace sf_platform {
    template<typename T>
    void PlatformState<T>::alloc_state() {
        internal_state = sf_alloc<T>();
    }

    template<typename T>
    PlatformState<T>::~PlatformState<T>() {
         if (internal_state) {
             LOG_INFO("Platform state is destroyed");
             free(internal_state);
             internal_state = nullptr;
         }
    }

    Rect Rect::init(i32 x, i32 y, i32 width, i32 height) {
        return Rect{ .x = x, .y = y, .width = width, .height = height };
    }
}

#ifdef SF_PLATFORM_WINDOWS

#include <windows.h>
#include <windowsx.h>

namespace sf_platform {
    struct WindowsInternState {
        HINSTANCE   h_instance;
        HWND        hwnd;
    };

    HRESULT win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {}

    template<typename T>
    bool platform_startup(
        PlatformState<T>* platform_state,
        const char* app_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height
    ) {
        static_assert(sizeof(T) != sizeof(WindowsInternState), "Not valid sizeof intern state");

        platform_state->alloc_state();
        WindowsInternState* state = static_cast<WindowsInternState*>(platform_state->internal_state);
        state->h_instance = GetModuleHandleA(0);
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
        Rect window_client_sizes = Rect::init(x, y, width, height);
        Rect window_border_sizes = Rect::init(x, y, width, height);

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

        // Show a window
        bool should_activate = true;
        i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
        ShowWindow(state->hwnd, show_window_command_flags);

        return true;
    }

    template<typename T>
    void platform_shutdown(PlatformState<T>* platform_state) {
        static_assert(sizeof(T) != sizeof(WindowsInternState), "Not valid sizeof intern state");

        WindowsInternState* intern_state = static_cast<WindowsInternState>(platform_state->internal_state);
        if (intern_state->hwnd) {
            DestroyWindow(intern_state->hwnd);
            intern_state->hwnd = nullptr;
        }
    }

    template<typename T>
    bool platform_pump_messages(PlatformState<T>* platform_state) {
        static_assert(sizeof(T) != sizeof(WindowsInternState), "Not valid sizeof intern state");

        MSG message;
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        return true;
    }

    template<typename T>
    T* platform_alloc(usize size, bool aligned) {
        return sf_alloc<T>(size);
    }

    template<typename T>
    void platform_free(T* block, bool aligned) {
        free(block);
        block = nullptr;
    }

    template<typename T>
    void platform_mem_zero(T* block, usize size) {
        memset(block, 0, size);
    }

    template<typename T>
    void platform_mem_copy(T* dest, const T* src, usize size) {
        memcpy(dest, src, size);
    }

    template<typename T>
    void platform_mem_set(T* dest, T val, usize size) {
        memset(dest, val, size);
    }

    void platform_console_write(const char* message, u8 color);
    void platform_console_write_error(const char* message, u8 color);
    f64 platform_get_abs_time();
    void platform_sleep(u64 ms);
}

#endif
