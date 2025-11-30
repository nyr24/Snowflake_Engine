#include "sf_core/utility.hpp"
#include "sf_core/logger.hpp"
#include "sf_platform/platform.hpp"

namespace sf {

[[noreturn]] void panic(const char* message) {
    LOG_FATAL("{}", message);
    std::exit(1);
}

[[noreturn]] void unreachable() {
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
    __assume(false);
#else // GCC, Clang
    __builtin_unreachable();
#endif
}

u32 get_mem_page_size() {
    static u32 mem_page_size = 0;
    if (mem_page_size == 0) {
        return platform_get_mem_page_size();
    }
    return mem_page_size;
}

} // sf
