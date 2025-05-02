#pragma once
#include <cstring>
#include <string_view>
#include "sf_platform_macros.hpp"
#include "sf_types.hpp"
#include "sf_util.hpp"

namespace sf_platform {
    struct PlatformState {
        void* internal_state;

        static PlatformState init_empty() {
            return PlatformState{ .internal_state = nullptr };
        }

        template<typename T>
        void alloc_inner_state() {
            internal_state = sf_alloc<T>(1, true);
        }

        void destroy() {}

        bool has_state() const { return internal_state != nullptr; }
    };

    struct Rect {
        i32 x;
        i32 y;
        i32 width;
        i32 height;

        Rect init(i32 x, i32 y, i32 width, i32 height) {
            return Rect{ .x = x, .y = y, .width = width, .height = height };
        }
    };

    SF_EXPORT bool platform_startup(
        PlatformState* platform_state,
        const char* app_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height
    );
    SF_EXPORT void platform_shutdown(PlatformState* platform_state);
    SF_EXPORT bool platform_pump_messages();
    void platform_console_write(std::string_view message, u8 color);
    void platform_console_write_error(std::string_view message, u8 color);
    f64 platform_get_abs_time();
    void platform_sleep(u64 ms);

#ifdef SF_PLATFORM_WINDOWS
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
#endif // SF_PLATFORM_WINDOWS
}
