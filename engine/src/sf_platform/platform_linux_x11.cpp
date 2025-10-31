#include "sf_platform/defines.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
#include "sf_platform/platform.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_containers/fixed_array.hpp"
#include <iostream>
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <sys/time.h>
#include <xcb/xproto.h>
#include <time.h> // nanosleep
#include <unistd.h> // usleep

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h>

namespace sf {

void* platform_mem_alloc(u64 byte_size, u16 alignment = 0) {
    if (alignment) {
        SF_ASSERT_MSG(is_power_of_two(alignment), "alignment should be a power of two");
        void* ptr = ::operator new(byte_size, static_cast<std::align_val_t>(alignment), std::nothrow);
        if (!ptr) {
            panic("Out of memory");
        }
        return ptr;
    } else {
        void* ptr = ::operator new(byte_size, std::nothrow);
        if (!ptr) {
            panic("Out of memory");
        }
        return ptr;
    }
}

static const FixedArray<std::string_view, 6> color_strings = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};

void platform_console_write(char* message_buff, u16 written_count, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    char message_buff2[OUTPUT_PRINT_BUFFER_CAPACITY] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff2, OUTPUT_PRINT_BUFFER_CAPACITY, "\033[{}m{}\033[0m", color_strings[color], const_cast<const char*>(message_buff));
    std::cout << std::string_view(const_cast<const char*>(message_buff2), res.out);
}

void platform_console_write_error(char* message_buff, u16 written_count, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    char message_buff2[OUTPUT_PRINT_BUFFER_CAPACITY] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff2, OUTPUT_PRINT_BUFFER_CAPACITY, "\033[{}m{}\033[0m", color_strings[color], const_cast<const char*>(message_buff));
    std::cerr << std::string_view(const_cast<const char*>(message_buff2), res.out);
}

f64 platform_get_abs_time() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 0.000000001;
}

void platform_sleep(u64 ms) {
#if _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000 * 1000;
    nanosleep(&ts, 0);
#else
    if (ms >= 1000) {
        sleep(ms / 1000);
    }
    usleep((ms % 1000) * 1000);
#endif
}

u32 platform_get_mem_page_size() {
    return static_cast<u32>(sysconf(_SC_PAGESIZE));
}

void platform_get_required_extensions(FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& required_extensions) {
    required_extensions.append("VK_KHR_xcb_surface");
}

} // sf

#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
