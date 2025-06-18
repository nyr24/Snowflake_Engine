#include "sf_core/memory_sf.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/logger.hpp"
#include <array>
#include <string_view>
#include <cstring>
#include <format>

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
    "SCENE"
};

ArenaAllocator::ArenaAllocator()
    : buffer_size{ platform_get_mem_page_size() }
    , buffer{static_cast<u8*>(sf_mem_alloc(buffer_size)) }
    , offset{ 0 }
{}

ArenaAllocator::ArenaAllocator(u64 buffer_size_)
    : buffer_size{ buffer_size_ }
    , buffer{static_cast<u8*>(sf_mem_alloc(buffer_size)) }
    , offset{ 0 }
{}

ArenaAllocator::ArenaAllocator(ArenaAllocator&& rhs) noexcept
    : buffer{ rhs.buffer }
    , buffer_size{ rhs.buffer_size }
    , offset{ rhs.offset }
{
    rhs.buffer = nullptr;
    rhs.buffer_size = 0;
    rhs.offset = 0;
}

ArenaAllocator::~ArenaAllocator()
{
    sf_mem_free(buffer, buffer_size);
}

void ArenaAllocator::reallocate(u64 new_size) {
    buffer_size = new_size;
    u8* new_buffer = static_cast<u8*>(sf_mem_alloc(new_size));
    sf::platform_mem_copy(new_buffer, buffer, offset);
    sf_mem_free(buffer, buffer_size);
    buffer = new_buffer;
}

SF_EXPORT void* sf_mem_alloc(u64 byte_size, u16 alignment, MemoryTag tag) {
    if (tag == MemoryTag::MEMORY_TAG_UNKNOWN) {
        LOG_INFO("unknown memory tag used for mem_alloc, please assign other tag later");
    }

    void* block = sf::platform_mem_alloc(byte_size, alignment);
    sf::platform_mem_zero(block, byte_size);

    mem_stats.total_allocated += byte_size;
    mem_stats.tag_allocated[tag] += byte_size;

    return block;
}

SF_EXPORT void sf_mem_free(void* block, u64 byte_size, u16 alignment, MemoryTag tag) {
    if (tag == MemoryTag::MEMORY_TAG_UNKNOWN) {
        LOG_INFO("unknown memory tag used for mem_free, please assign other tag later");
    }

    sf::platform_mem_free(block, alignment);

    mem_stats.total_allocated -= byte_size;
    mem_stats.tag_allocated[tag] -= byte_size;
}

SF_EXPORT void sf_mem_copy(void* dest, const void* src, u64 byte_size) {
    sf::platform_mem_copy(dest, src, byte_size);
}

SF_EXPORT void sf_mem_set(void* dest, i32 value, u64 byte_size) {
    sf::platform_mem_set(dest, byte_size, value);
}

SF_EXPORT void sf_mem_zero(void* block, u64 byte_size) {
    sf::platform_mem_zero(block, byte_size);
}

SF_EXPORT i8* get_memory_usage_str() {
    constexpr u64 gib = 1024 * 1024 * 1024;
    constexpr u64 mib = 1024 * 1024;
    constexpr u64 kib = 1024;

    constexpr u32 BUFF_LEN{ 8000 };
    i8 buffer[BUFF_LEN] = "System memory use (tagged):\n";
    u64 offset = strlen(buffer);

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
        offset += res.size;
    }

    i8* out_string = strdup(buffer);

    return out_string;
}

} // sf
