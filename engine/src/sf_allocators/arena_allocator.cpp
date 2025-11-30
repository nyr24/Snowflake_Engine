#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/traits.hpp"
#include "sf_core/application.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/constants.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_allocators/arena_allocator.hpp"
#include <algorithm>

namespace sf {

ArenaAllocator::ArenaAllocator()
    : regions(DEFAULT_REGIONS_INIT_CAPACITY, &application_get_gpa())
{
}

void* ArenaAllocator::allocate(usize size, u16 alignment) {
    auto [region, padding] = find_sufficient_region_for_alloc(size, alignment);

    if (region->data == nullptr) {
        init_new_region(region, size + padding);
    }

    void* return_ptr = static_cast<void*>(region->data + region->offset + padding);
    ArenaAllocatorHeader* header = ptr_step_bytes_backward<ArenaAllocatorHeader>(return_ptr, sizeof(ArenaAllocatorHeader));
    header->padding = padding;
    header->diff = region->offset - region->prev_offset;
    
    region->prev_offset = region->offset;
    region->offset += padding + size;
    return return_ptr;
}

usize ArenaAllocator::allocate_handle(usize size, u16 alignment) {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return INVALID_ALLOC_HANDLE;
}

ReallocReturn ArenaAllocator::reallocate(void* ptr, usize new_size, u16 alignment) {
    if (new_size == 0) {
        return {nullptr, false};
    }
    if (!ptr) {
        return {allocate(new_size, alignment), false};
    }

    Region* region = find_region_for_addr(ptr);
    if (!region) {
        return {nullptr, false};
    }

    ArenaAllocatorHeader* header = ptr_step_bytes_backward<ArenaAllocatorHeader>(ptr, sizeof(ArenaAllocatorHeader));
    u32 prev_offset = turn_ptr_into_handle(ptr_step_bytes_backward(ptr, header->padding), region->data);
    if (prev_offset != region->prev_offset) {
        return {allocate(new_size, alignment), true};
    }

    u32 prev_alloc_size = region->offset - region->prev_offset;
    u32 alloc_diff = new_size - prev_alloc_size;

    // shrink last alloc
    if (alloc_diff < 0) {
        region->offset += alloc_diff;
        return {ptr, false};
    }
    
    u32 remain_mem = region->capacity - region->offset;

    // enough space case
    if (alloc_diff <= remain_mem) {
        region->offset += alloc_diff; 
        return {ptr, false};
    }

    // not enough space - reallocate to new region

    // free old alloc
    free_inside_region(ptr, region);

    // allocate inside new region
    auto [new_region, padding] = find_sufficient_region_for_alloc(new_size, alignment);

    if (region->data == nullptr) {
        init_new_region(region, new_size + padding);
    }

    void* return_ptr = static_cast<void*>(region->data + region->offset + padding);
    header = ptr_step_bytes_backward<ArenaAllocatorHeader>(return_ptr, sizeof(ArenaAllocatorHeader));
    header->padding = padding;
    header->diff = region->offset - region->prev_offset;
    
    region->prev_offset = region->offset;
    region->offset += padding + new_size;
    return {return_ptr, true};
}

ArenaAllocator::Region* ArenaAllocator::find_region_for_addr(void* addr) {
    if (regions.count() == 0) {
        return nullptr;
    }
    
    u32 region_ind{0};
    Region* region;
    
    for (; region_ind < regions.count(); ++region_ind) {
        region = &regions[region_ind];
        if (is_address_in_range(region->data, region->capacity, addr)) {
            return region;
        }
    }

    return nullptr;
}

ArenaAllocator::FindSufficcientRegionReturn ArenaAllocator::find_sufficient_region_for_alloc(u32 alloc_size, u16 alignment) {
    FindSufficcientRegionReturn res;
    u32 region_index{0};
    const u32 reg_cnt = regions.count();

    for (; region_index < reg_cnt; ++region_index) {
        res.region = &regions[region_index];
        if (!res.region->data) {
            break;
        }
        
        res.padding = calc_padding_with_header(res.region->data + res.region->offset, alignment, sizeof(ArenaAllocatorHeader));
        u32 required_mem = alloc_size + res.padding;
        u32 remain_mem = res.region->capacity - res.region->offset;
        
        if (required_mem <= remain_mem) {
            break;
        }
    }

    if (region_index >= regions.count()) {
        regions.append(Region{});
        res.region = regions.last_ptr();
        res.padding = 0;
    }

    return res;
}

void ArenaAllocator::init_new_region(Region* region, usize alloc_size) {
    const usize alloc_size_ = std::max(alloc_size, get_mem_page_size() * static_cast<usize>(DEFAULT_REGION_CAPACITY_PAGES));
    region->data = static_cast<u8*>(sf_mem_alloc(alloc_size, DEFAULT_ALIGNMENT));
    region->offset = 0;
    region->prev_offset = 0;
    region->capacity = alloc_size;
}

ReallocReturnHandle ArenaAllocator::reallocate_handle(usize handle, usize size, u16 alignment) {
    SF_ASSERT_MSG(false, "You are using ArenaAllocator with handles");
    return {INVALID_ALLOC_HANDLE, false};
}

void ArenaAllocator::free(void* addr)
{
    Region* region = find_region_for_addr(addr);
    if (!region) {
        return;
    }

    ArenaAllocatorHeader* header = ptr_step_bytes_backward<ArenaAllocatorHeader>(addr, sizeof(ArenaAllocatorHeader));
    u32 prev_offset = turn_ptr_into_handle(ptr_step_bytes_backward(addr, header->padding), region->data);
    if (prev_offset != region->prev_offset) {
        return;
    }

    region->offset = region->prev_offset;
    region->prev_offset -= header->diff; 
}

void ArenaAllocator::free_inside_region(void* addr, Region* region) {
    ArenaAllocatorHeader* header = ptr_step_bytes_backward<ArenaAllocatorHeader>(addr, sizeof(ArenaAllocatorHeader));
    u32 prev_offset = turn_ptr_into_handle(ptr_step_bytes_backward(addr, header->padding), region->data);
    if (prev_offset != region->prev_offset) {
        return;
    }

    region->offset = region->prev_offset;
    region->prev_offset -= header->diff; 
}

void ArenaAllocator::clear() {
    for (auto& region : regions) {
        region.offset = 0;
    }
}

void ArenaAllocator::reserve(usize needed_capacity) {
    Region* region;
    u32 founded_region{regions.count()};

    for (i32 i{0}; i < regions.count(); ++i) {
        region = &regions[i];
        if (!region->data) {
            founded_region = i;
        }
        else if ((region->capacity - region->offset) >= needed_capacity) {
            founded_region = i;
        }
    }

    if (founded_region == regions.count()) {
        regions.append(Region{});
        region = regions.last_ptr();
    }

    const usize alloc_size = std::max(needed_capacity, get_mem_page_size() * static_cast<usize>(DEFAULT_REGION_CAPACITY_PAGES));
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


} // sf

