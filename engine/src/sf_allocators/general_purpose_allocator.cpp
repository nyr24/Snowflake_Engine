#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/optional.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

void* GeneralPurposeAllocator::allocate(u32 size, u16 alignment) noexcept {
    return sf_mem_alloc(size, alignment);
}

usize GeneralPurposeAllocator::allocate_handle(u32 size, u16 alignment) noexcept {
    return reinterpret_cast<usize>(sf_mem_alloc(size, alignment));
}

void* GeneralPurposeAllocator::reallocate(void* addr, u32 new_size, u16 alignment) noexcept {
    return sf_mem_realloc(addr, new_size);
}

Option<usize> GeneralPurposeAllocator::reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept {
    if (handle == INVALID_ALLOC_HANDLE) {
        return allocate_handle(new_size, alignment);    
    }
    
    return reinterpret_cast<usize>(reallocate(get_mem_with_handle(handle), new_size, alignment));
}

void GeneralPurposeAllocator::free(void* addr) noexcept {
    sf_mem_free(addr);
    addr = nullptr;
}

void* GeneralPurposeAllocator::get_mem_with_handle(usize handle) noexcept {
    if (handle == INVALID_ALLOC_HANDLE) {
        return nullptr;
    }

    return reinterpret_cast<void*>(handle);
}

void GeneralPurposeAllocator::free_handle(usize handle) noexcept {
    void* ptr = get_mem_with_handle(handle);
    sf_mem_free(ptr);
    ptr = nullptr;
}

} // sf
