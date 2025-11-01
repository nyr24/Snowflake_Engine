#pragma once

#include "sf_core/defines.hpp"

namespace sf {

struct GeneralPurposeAllocator { 
    void* allocate(u32 size, u16 alignment) noexcept;
    usize allocate_handle(u32 size, u16 alignment) noexcept;
    void* get_mem_with_handle(usize handle) noexcept;
    void* reallocate(void* addr, u32 new_size, u16 alignment) noexcept;
    usize reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept;
    void free(void* addr) noexcept;
    void free_handle(usize handle) noexcept;
    void clear() noexcept {}
};

} // sf
