#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_platform/platform.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include <algorithm>

namespace sf {

ArenaAllocator::ArenaAllocator()
    : regions(DEFAULT_REGIONS_INIT_CAPACITY)
{}

void* ArenaAllocator::allocate(usize size, u16 alignment) {
    const usize aligned_size = (size + alignment - 1) & ~(alignment - 1);
    Region* region;
    bool found_region = false;

    for (u32 i{0}; i < regions.count(); ++i) {
        region = &regions[i];
        if (region->data == nullptr || (region->offset + aligned_size) <= region->capacity) {
            found_region = true;
            break;
        }
    }

    if (!found_region) {
        regions.append(Region{});
        region = regions.last_ptr();
    }

    if (region->data == nullptr) {
        const usize alloc_size = std::max(aligned_size, platform_get_mem_page_size() * static_cast<usize>(DEFAULT_REGION_CAPACITY_PAGES));
        region->data = static_cast<u8*>(sf_mem_alloc(alloc_size, DEFAULT_ALIGNMENT));
        region->offset = 0;
        region->capacity = alloc_size;
    }

    void* return_ptr = static_cast<void*>(region->data + region->offset);
    region->offset += aligned_size;
    return return_ptr;
}

usize ArenaAllocator::allocate_handle(usize size, u16 alignment) {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return INVALID_ALLOC_HANDLE;
}

ReallocReturn ArenaAllocator::reallocate(void* ptr, usize size, u16 alignment) {
    if (size == 0) {
        return {nullptr, false};
    }

    return {allocate(size, alignment), true};
}

ReallocReturnHandle ArenaAllocator::reallocate_handle(usize handle, usize size, u16 alignment) {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return {INVALID_ALLOC_HANDLE, false};
}

void ArenaAllocator::free(void* addr)
{
}

void ArenaAllocator::free_handle(usize handle)
{
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
}

void* ArenaAllocator::handle_to_ptr(usize handle) const {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return nullptr;
}

usize ArenaAllocator::ptr_to_handle(void* ptr) const {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return INVALID_ALLOC_HANDLE;
}

void ArenaAllocator::clear() {
    for (auto& region : regions) {
        region.offset = 0;
    }
}

void ArenaAllocator::reserve(usize needed_capacity) {
    Region* region;
    i32 founded_region{-1};

    for (i32 i{0}; i < regions.count(); ++i) {
        region = &regions[i];
        if (!region->data) {
            founded_region = i;
        }
        else if ((region->offset - region->capacity) >= needed_capacity) {
            founded_region = i;
        }
    }

    if (founded_region == -1) {
        regions.append(Region{});
        region = regions.last_ptr();
    }

    const usize alloc_size = std::max(needed_capacity, platform_get_mem_page_size() * static_cast<usize>(DEFAULT_REGION_CAPACITY_PAGES));
    region->data = static_cast<u8*>(sf_mem_alloc(alloc_size, DEFAULT_ALIGNMENT));
    region->offset = 0;
    region->capacity = alloc_size;
}

ArenaAllocator::~ArenaAllocator() {
    for (const auto& r : regions) {
        sf_mem_free(r.data, DEFAULT_ALIGNMENT);
    }
}

void ArenaAllocator::rewind(Snapshot snapshot) {
    if (snapshot.region_index < regions.count())
    {
        Region* r = &regions[snapshot.region_index];
        r->offset = snapshot.region_offset;
        for (usize i = snapshot.region_index + 1; i < regions.count(); ++i)
        {
            regions[i].offset = 0;
        }
    }
}

ArenaAllocator::Snapshot ArenaAllocator::make_snapshot() const {
    Snapshot s;
    if (regions.count() > 0) {
        s.region_index = regions.count() - 1;
        s.region_offset = regions[s.region_index].offset;
    } else {
        s.region_index = 0;
        s.region_offset = 0;
    }
    return s;
}

} // sf

