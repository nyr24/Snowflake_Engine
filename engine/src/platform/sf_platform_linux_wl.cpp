#include "platform/sf_platform.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
#include "core/sf_logger.hpp"
#include <wayland-client-core.h>
#include <wayland-client.h>

namespace sf_platform {
    struct WaylandInternState {
        wl_display* display;
    };

    PlatformState PlatformState::init() {
        return PlatformState{ .internal_state = nullptr };
    }

    bool PlatformState::startup(
        const char* app_name,
        i32 x,
        i32 y,
        i32 width,
        i32 height
    ) {
        this->alloc_inner_state<WaylandInternState>();
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        state->display = wl_display_connect(nullptr);
        if (!state->display) {
            LOG_FATAL("Failed to connect to Wayland display");
            return false;
        }
        LOG_INFO("Connection established!");
        return true;
    }

    PlatformState::~PlatformState() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        if (state->display) {
            wl_display_disconnect(state->display);
            state->display = nullptr;
            free(state);
        }
    }

    bool PlatformState::start_event_loop() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        while (wl_display_dispatch(state->display)) {
        }

        return true;
    }

    void            platform_console_write(std::string_view message, u8 color);
    void            platform_console_write_error(std::string_view message, u8 color);
    f64             platform_get_abs_time();
    void            platform_sleep(u64 ms);
}
#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
