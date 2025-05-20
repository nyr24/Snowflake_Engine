#include "platform/sf_platform.hpp"

#ifdef SF_PLATFORM_WINDOWS
#include <cstring>
#include <string_view>
#include <Windows.h>
#include <windowsx.h>
#include "core/sf_types.hpp"

namespace sf_platform {
    struct WindowsInternState {
        HINSTANCE   h_instance;
        HWND        hwnd;
    };

    static f64 clock_frequency;
    static LARGE_INTEGER start_time;

    HRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

    PlatformState PlatformState::init() {
        auto state = PlatformState{ .internal_state = sf_alloc<WindowsInternState>(1, true) };
        std::memset(state.internal_state, 0, sizeof(state.internal_state));
        return state;
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
            free(intern_state);
        }
    }

    bool PlatformState::start_event_loop() {
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

    void platform_console_write(std::string_view message, u8 color) {
        HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        static u8 levels[6] = { 64, 4, 6, 2, 1, 8 };
        SetConsoleTextAttribute(console_handle, levels[color]);

        OutputDebugStringA(message.data());
        LPDWORD count_written = 0;
        WriteConsoleA(console_handle, message, (DWORD)message.length(), count_written, 0);
    }

    void platform_console_write_error(std::string_view message, u8 color) {
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

    HRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {
        switch (msg) {
            case WM_ERASEBKGND:
                // will be handled by the application
                return 1;
            case WM_CLOSE:
                // will be handled by the application
                // TODO: fire event for the app to quit
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

                // TODO: fire an event for window resize
            } break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN;
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                bool pressed = WM_KEYDOWN || WM_SYSKEYDOWN;
                // TODO: process input
            } break;
            case WM_MOUSEMOVE: {
                i32 x_pos = GET_X_PARAM(l_param);
                i32 y_pos = GET_Y_PARAM(l_param);
                // TODO: process input
            } break;
            case MOUSEWHEEL: {
                i32 z_delta = GET_WHEEL_DELTA_PARAM(w_param);
                if (z_delta != 0) {
                    z_delta = z_delta < 0 ? -1 : 1;
                }
                // TODO: process input
            } break;
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP: {
                bool pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
                // TODO: process input
            } break;
            default: return DefWindowProc(hwnd, msg, w_param, l_param);
        }
    }
}
#endif // SF_PLATFORM_WINDOWS
