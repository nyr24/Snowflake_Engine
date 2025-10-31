#include "sf_platform/defines.hpp"

#if defined(SF_PLATFORM_WINDOWS)
#include "sf_platform/platform.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/logger.hpp"
#include "sf_containers/fixed_array.hpp"
#include <Windows.h>
#include <windowsx.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

namespace sf {

void* platform_mem_alloc(u64 byte_size, u16 alignment = 0) {
    HANDLE heap_handle = GetProcessHeap();
    void* ptr = HeapAlloc(heap_handle, HEAP_ZERO_MEMORY, byte_size);
    if (!ptr) {
        panic("Out of memory");
    }
    return ptr;
}

f64 platform_get_abs_time() {
    LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (f64)now_time.QuadPart * clock_frequency;
}

static const FixedArray<u8, static_cast<u8>(LogLevel::COUNT)> levels = { 64, 4, 6, 2, 1, 8 };

void platform_console_write(char* message_buff, u16 written_count, u8 color) {
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(console_handle, levels[color]);

    OutputDebugStringA(message_buff);
    DWORD res_count_written;
    WriteConsoleA(console_handle, message_buff, (DWORD)written_count, &res_count_written, 0);
}

void platform_console_write_error(char* message_buff, u16 written_count, u8 color) {
    HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(console_handle, levels[color]);

    OutputDebugStringA(message_buff);
    DWORD res_count_written;
    WriteConsoleA(console_handle, message_buff, (DWORD)written_count, &res_count_written, 0);
}

void platform_sleep(u64 ms) {
    Sleep(ms);
}

u32 platform_get_mem_page_size() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<u32>(si.dwPageSize);
}

void platform_get_required_extensions(FixedArray<const char*, VK_MAX_EXTENSION_COUNT>& required_extensions) {
    required_extensions.append("VK_KHR_win32_surface");
}

} // sf

#endif // SF_PLATFORM_WINDOWS
