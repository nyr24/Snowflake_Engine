#include "sf_allocators/free_list_allocator.hpp"
#include "sf_containers/result.hpp"

namespace sf {

bool start_tests() {
    FreeList<{ true, true }> list(1024);
    Result<u32> h1 = list.allocate_block_and_return_handle(448, 8);
    Result<u32> h3 = list.allocate_block_and_return_handle(224, 8);
    Result<u32> h2 = list.allocate_block_and_return_handle(628, 8);
    Result<u32> h4 = list.allocate_block_and_return_handle(94, 8);

    list.free_block_with_handle(h1.data);
    list.free_block_with_handle(h3.data);
    list.free_block_with_handle(h2.data);
    list.free_block_with_handle(h4.data);

    return true;
}

bool res = start_tests();

} // sf
