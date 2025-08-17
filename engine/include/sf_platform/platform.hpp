#pragma once

#include "sf_core/defines.hpp"
#include "sf_containers/fixed_array.hpp"
#include <vulkan/vulkan_core.h>

namespace sf {
struct ApplicationState;

struct PlatformState {
public:
    void* internal_state;

public:
    PlatformState();
    PlatformState(PlatformState&& rhs) noexcept;
    PlatformState& operator=(PlatformState&& rhs) noexcept;
    PlatformState(const PlatformState& rhs) = delete;
    PlatformState& operator=(const PlatformState& rhs) = delete;
    ~PlatformState();

    bool startup(
        const char* app_name,
        u32 x,
        u32 y,
        u32 width,
        u32 height
    );

    bool start_event_loop(ApplicationState& app_state);
};

struct PlatformState;
struct VulkanContext;

// non-templated versions of memory functions (needed for void*)
void*   platform_mem_alloc(u64 byte_size, u16 alignment);
u32     platform_get_mem_page_size();
void    platform_console_write(const i8* message, u8 color);
void    platform_console_write_error(const i8* message, u8 color);
f64     platform_get_abs_time();
void    platform_sleep(u64 ms);
void    platform_get_required_extensions(FixedArray<const char*, 5>& required_extensions);
void    platform_create_vk_surface(PlatformState& plat_state, VulkanContext& context);

} // sf
