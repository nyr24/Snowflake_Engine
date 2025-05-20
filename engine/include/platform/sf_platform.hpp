#pragma once
#include <string_view>
#include "platform/sf_platform_macros.hpp"
#include "core/sf_types.hpp"
#include <cstdlib>
#include <cstring>
// TODO: 
namespace sf_platform {
    struct PlatformState {
        static PlatformState init();

        bool startup(
            const char* app_name,
            i32 x,
            i32 y,
            i32 width,
            i32 height
        );

        ~PlatformState();

        bool start_event_loop();

        void* internal_state = nullptr;
    };

    struct Rect {
        i32 x;
        i32 y;
        i32 width;
        i32 height;

        static Rect init(i32 x, i32 y, i32 width, i32 height) {
            return Rect{ .x = x, .y = y, .width = width, .height = height };
        }
    };

    void            platform_console_write(std::string_view message, u8 color);
    void            platform_console_write_error(std::string_view message, u8 color);
    f64             platform_get_abs_time();
    void            platform_sleep(u64 ms);

// #ifdef SF_PLATFORM_WINDOWS
    template<typename T>
    T* platform_alloc(usize count, bool aligned) {
        return sf_alloc<T>(count, aligned);
    }

    template<typename T>
    void platform_free(T* block, bool aligned) {
        free(block);
        block = nullptr;
    }

    template<typename T>
    void platform_mem_zero(T* block, usize size) {
        std::memset(block, 0, size);
    }

    template<typename T>
    void platform_mem_copy(T* dest, const T* src, usize size) {
        std::memcpy(dest, src, size);
    }

    template<typename T>
    void platform_mem_set(T* dest, T val, usize size) {
        std::memset(dest, val, size);
    }
// #endif // SF_PLATFORM_WINDOWS
}
