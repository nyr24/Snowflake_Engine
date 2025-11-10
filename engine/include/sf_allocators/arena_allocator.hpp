#pragma once

#include "sf_containers/traits.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/dynamic_array.hpp"

namespace sf {

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
    };
private:
    DynamicArray<Region> regions;
    u16                  current_region_index;
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
};

} // sf
