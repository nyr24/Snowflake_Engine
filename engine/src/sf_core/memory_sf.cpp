#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/utility.hpp"
#include "sf_platform/platform.hpp"
#include <cstdlib>
#include <new>
#include <cstring>

namespace sf {

SF_EXPORT void* sf_mem_alloc(usize byte_size, u16 alignment) {
    void* block = platform_mem_alloc(byte_size, alignment);
#ifdef SF_DEBUG
    sf_mem_zero(block, byte_size);
#endif
    return block;
}

SF_EXPORT void* sf_mem_realloc(void* ptr, usize byte_size) {
    void* block = std::realloc(ptr, byte_size);
    if (!block) {
        panic("Out of memory");
    }
#ifdef SF_DEBUG
    sf_mem_zero(block, byte_size);
#endif
    return block;
}

SF_EXPORT void sf_mem_free(void* block, u16 alignment) {
    ::operator delete(block, static_cast<std::align_val_t>(alignment), std::nothrow);
}

SF_EXPORT void sf_mem_set(void* block, usize byte_size, i32 value) {
    std::memset(block, value, byte_size);
}

SF_EXPORT void sf_mem_zero(void* block, usize byte_size) {
    sf_mem_set(block, byte_size, 0);
}

SF_EXPORT void sf_mem_copy(void* dest, void* src, usize byte_size) {
    std::memcpy(dest, src, byte_size);
}

SF_EXPORT void sf_mem_move(void* dest, void* src, usize byte_size) {
    std::memmove(dest, src, byte_size);
}

SF_EXPORT bool sf_mem_cmp(void* first, void* second, usize byte_size) {
    return std::memcmp(first, second, byte_size) == 0;
}

SF_EXPORT bool sf_str_cmp(const char* first, const char* second) {
    return std::strcmp(first, second) == 0;
}

} // sf
