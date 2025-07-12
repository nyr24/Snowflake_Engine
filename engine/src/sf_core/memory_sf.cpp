#include "sf_core/memory_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_platform/platform.hpp"
#include <array>
#include <new>
#include <string_view>
#include <cstring>
#include <format>
#include <iostream>

namespace sf {
struct MemoryStats {
    u64 total_allocated;
    std::array<u64, MemoryTag::MEMORY_TAG_MAX_TAGS> tag_allocated;
};

static MemoryStats mem_stats;

static std::array<std::string_view, MemoryTag::MEMORY_TAG_MAX_TAGS> memory_tag_strings = {
    "UNKNOWN",
    "APPLICATION",
    "JOB",
    "TEXTURE",
    "MAT_INST",
    "RENDERER",
    "GAME",
    "TRANSFORM",
    "ENTITY",
    "ENTITY_NODE",
    "SCENE",
};

SF_EXPORT void* sf_mem_alloc(u64 byte_size, u16 alignment, MemoryTag tag) {
#ifdef SF_DEBUG
    if (tag == MemoryTag::MEMORY_TAG_UNKNOWN) {
        // LOG_INFO("unknown memory tag used for mem_alloc, please assign other tag later");
    }
#endif

    void* block = sf::platform_mem_alloc(byte_size, alignment);
    sf_mem_set(block, byte_size, 0);

#ifdef SF_DEBUG
    mem_stats.total_allocated += byte_size;
    mem_stats.tag_allocated[tag] += byte_size;
#endif

    return block;
}

SF_EXPORT void sf_mem_free(void* block, u64 byte_size, u16 alignment, MemoryTag tag) {
#ifdef SF_DEBUG
    if (tag == MemoryTag::MEMORY_TAG_UNKNOWN) {
        // LOG_INFO("unknown memory tag used for mem_free, please assign other tag later");
    }
#endif

    ::operator delete(block, static_cast<std::align_val_t>(alignment), std::nothrow);

#ifdef SF_DEBUG
    mem_stats.total_allocated -= byte_size;
    mem_stats.tag_allocated[tag] -= byte_size;
#endif
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

SF_EXPORT bool sf_mem_cmp(void* first, void* second, usize byte_size) {
    return std::memcmp(first, second, byte_size) == 0;
}

SF_EXPORT i8* get_memory_usage_str() {
    constexpr u64 gib = 1024 * 1024 * 1024;
    constexpr u64 mib = 1024 * 1024;
    constexpr u64 kib = 1024;

    constexpr u32 BUFF_LEN{ 8000 };
    i8 buffer[BUFF_LEN] = "System memory use (tagged):\n";
    u64 len = strlen(buffer);

    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) {
        i8 unit[4] = "XiB";
        f32 amount = 1.0f;

        if (mem_stats.tag_allocated[i] >= gib) {
            unit[0] = 'G';
            amount = mem_stats.tag_allocated[i] / static_cast<f32>(gib);
        } else if (mem_stats.tag_allocated[i] >= mib) {
            unit[0] = 'M';
            amount = mem_stats.tag_allocated[i] / static_cast<f32>(mib);
        } else if (mem_stats.tag_allocated[i] >= kib) {
            unit[0] = 'K';
            amount = mem_stats.tag_allocated[i] / static_cast<f32>(kib);
        } else {
            unit[0] = 'B';
            unit[1] = 0;
            amount = static_cast<f32>(mem_stats.tag_allocated[i]);
        }

        const std::format_to_n_result res = std::format_to_n(buffer, BUFF_LEN, "  {}: {}{}\n", memory_tag_strings[i], amount, unit);
        std::cout << std::fixed << std::string_view(const_cast<const i8*>(buffer), res.out);
        len += res.size;
    }

    i8* out_string = strdup(buffer);

    return out_string;
}

} // sf
