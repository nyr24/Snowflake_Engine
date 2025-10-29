#include "sf_core/asserts_sf.hpp"
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

u32 sf_calc_padding(void* address, u16 alignment) {
    void* aligned_addr = sf_align_forward(address, alignment);
    return reinterpret_cast<usize>(aligned_addr) - reinterpret_cast<usize>(address);
}

bool is_address_in_range(void* start, u32 total_size, void* addr) {
    usize start_v = reinterpret_cast<usize>(start);
    usize addr_v = reinterpret_cast<usize>(addr);
    usize end = start_v + total_size;
    return addr_v >= start_v && addr_v < end;
}

bool is_handle_in_range(void* start, u32 total_size, u32 handle) {
    usize start_v = reinterpret_cast<usize>(start);
    usize addr_v = reinterpret_cast<usize>(turn_handle_into_ptr(handle, start));
    usize end = start_v + total_size;
    return addr_v >= start_v && addr_v < end;
}

u32 turn_ptr_into_handle(void* ptr, void* start) {
    usize offset_v = reinterpret_cast<usize>(ptr);
    usize start_v = reinterpret_cast<usize>(start);

    SF_ASSERT_MSG(offset_v >= start_v, "start_v can't be bigger than ptr");

    return offset_v - start_v;
}

u32 ptr_diff(void* ptr1, void* ptr2) {
    usize val_1 = reinterpret_cast<usize>(ptr1);
    usize val_2 = reinterpret_cast<usize>(ptr2);

    if (val_1 > val_2) {
        return val_1 - val_2;
    } else {
        return val_2 - val_1;
    }
}

u32 calc_padding_with_header(void* ptr, u16 alignment, u16 header_size) {
	usize p, a, modulo, padding;

	p = reinterpret_cast<usize>(ptr);
	a = alignment;
	modulo = p & (a - 1); // (p % a) as it assumes alignment is a power of two

	padding = 0;

	if (modulo != 0) {
		padding = a - modulo;
	}

	if (padding < header_size) {
		header_size -= padding;

		if ((header_size & (a - 1)) != 0) {
			padding += a * (1 + (header_size / a));
		} else {
			padding += a * (header_size / a);
		}
	}

	return static_cast<u32>(padding);
}

} // sf
