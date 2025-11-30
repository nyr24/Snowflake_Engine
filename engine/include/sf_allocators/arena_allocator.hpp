#pragma once

#include "sf_allocators/general_purpose_allocator.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"

namespace sf {

struct ArenaAllocatorHeader {
    u32 padding;
    u32 diff;
};

struct ArenaAllocator {
public:
    static constexpr u32 DEFAULT_ALIGNMENT{sizeof(usize)};
    static constexpr u32 DEFAULT_REGIONS_INIT_CAPACITY{10};
    static constexpr u32 DEFAULT_REGION_CAPACITY_PAGES{4};

    struct Snapshot {
        u32 region_offset;
        u16 region_index;
    };

    struct Region {
        u8* data;
        u32 capacity;
        u32 offset;
        u32 prev_offset;
    };
private:
    DynamicArray<Region, GeneralPurposeAllocator, false> regions;
public:
    ArenaAllocator();
    ~ArenaAllocator();
    void* allocate(usize size, u16 alignment);
    usize allocate_handle(usize size, u16 alignment);
    ReallocReturn reallocate(void* ptr, usize size, u16 alignment);
    ReallocReturnHandle reallocate_handle(usize handle, usize size, u16 alignment);
    void* handle_to_ptr(usize handle) const;
    usize ptr_to_handle(void* ptr) const;
    void  free(void* addr);
    void  free_handle(usize hanlde);
    void  reserve(usize capacity);
    void  clear();
    void  rewind(Snapshot snapshot);
    Snapshot make_snapshot() const;
private:
    struct FindSufficcientRegionReturn {
        Region* region;
        u32 padding;  
    };
    FindSufficcientRegionReturn find_sufficient_region_for_alloc(u32 alloc_size, u16 alignment);
    Region* find_region_for_addr(void* addr);
    void init_new_region(Region* region, usize alloc_size);
    void free_inside_region(void* addr, Region* region);
};

} // sf
