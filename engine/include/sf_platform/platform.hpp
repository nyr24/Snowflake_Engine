#pragma once
#include "sf_core/util.hpp"
#include "sf_platform/platform_macros.hpp"
#include "sf_core/types.hpp"
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace sf_platform {
struct PlatformState {
    PlatformState();
    PlatformState(PlatformState&& rhs) noexcept;
    PlatformState& operator=(PlatformState&& rhs) noexcept;
    PlatformState(const PlatformState& rhs) = delete;
    PlatformState& operator=(const PlatformState& rhs) = delete;
    ~PlatformState();

    bool startup(
        const char* app_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height
    );
    bool start_event_loop();

    void* internal_state;
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
#endif // SF_PLATFORM_WINDOWS

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
template<typename T>
// NOTE: export to the user code is temporary
SF_EXPORT T* platform_alloc(usize count, bool aligned) {
    return sf_alloc<T>(count, aligned);
}

template<typename T>
// NOTE: export to the user code is temporary
SF_EXPORT void platform_free(T* block, bool aligned) {
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
#endif // SF_PLATFORM_WAYLAND

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
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
#endif // SF_PLATFORM_X11
}
