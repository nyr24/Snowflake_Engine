#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"

namespace sf {

void* GeneralPurposeAllocator::allocate(u32 size, u16 alignment) noexcept {
    return sf_mem_alloc(size, alignment);
}

ReallocReturn GeneralPurposeAllocator::reallocate(void* addr, u32 new_size, u16 alignment) noexcept {
    return {sf_mem_realloc(addr, new_size), false};
}

void GeneralPurposeAllocator::free(void* addr) noexcept {
    sf_mem_free(addr);
    addr = nullptr;
}

void GeneralPurposeAllocator::free_handle(usize handle) noexcept
{
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
}

void* GeneralPurposeAllocator::handle_to_ptr(usize handle) const noexcept {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return nullptr;
}

usize GeneralPurposeAllocator::ptr_to_handle(void* ptr) const noexcept {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return INVALID_ALLOC_HANDLE;
}

// usize GeneralPurposeAllocator::allocate_handle(u32 size, u16 alignment) noexcept {
//     return reinterpret_cast<usize>(sf_mem_alloc(size, alignment));
// }

// ReallocReturnHandle GeneralPurposeAllocator::reallocate_handle(usize handle, u32 new_size, u16 alignment) noexcept {
//     if (handle == INVALID_ALLOC_HANDLE) {
//         return {allocate_handle(new_size, alignment), false};
//     }

//     ReallocReturn realloc_res = reallocate(handle_to_ptr(handle), new_size, alignment);
//     return {reinterpret_cast<usize>(realloc_res.ptr), realloc_res.should_mem_copy};
// }

// void* GeneralPurposeAllocator::handle_to_ptr(usize handle) noexcept {
// #ifdef SF_DEBUG
//     if (handle == INVALID_ALLOC_HANDLE) {
//         return nullptr;
//     }
// #endif

//     return reinterpret_cast<void*>(handle);
// }

// usize GeneralPurposeAllocator::ptr_to_handle(void* ptr) noexcept {
// #ifdef SF_DEBUG
//     if (!ptr) {
//         return INVALID_ALLOC_HANDLE;
//     }
// #endif
//     return reinterpret_cast<usize>(ptr);
// }

// void GeneralPurposeAllocator::free_handle(usize handle) noexcept {
//     void* ptr = handle_to_ptr(handle);
//     sf_mem_free(ptr);
//     ptr = nullptr;
// }

} // sf
