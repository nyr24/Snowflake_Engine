#include "platform/sf_platform.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
#include "core/sf_logger.hpp"
#include <wayland-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include "platform/xdg-shell-protocol.hpp"

namespace sf_platform {
    constexpr u32 WIDTH = 720, HEIGHT = 480;
    constexpr u32 STRIDE = WIDTH * 4;
    constexpr u32 BUFF_COUNT = 2;
    constexpr u32 SHM_POOL_SIZE = HEIGHT * STRIDE * BUFF_COUNT;

    using wl_surface = struct wl_surface;
    using xdg_wm_base = struct xdg_wm_base;

    struct ShmPoolState {
        u8*             pool_data;
        wl_buffer*      buffers[2];
        wl_surface*     surface;
        xdg_surface*    xdg_surface;
        xdg_toplevel*   xdg_toplevel;
        u8              active_buffer = 0;
    };

    struct GlobalObjectsState {
        ShmPoolState        shm_pool_state;
        wl_compositor*      compositor;
        wl_shm*             shm;
        xdg_wm_base*        xdg_wm_base;
        const i8*           window_name;
    };

    struct Listeners {
        wl_registry_listener    registry_listener;
        wl_buffer_listener      buffer_listener;
    };

    struct WaylandInternState {
        GlobalObjectsState      go_state;
        Listeners               listeners;
        wl_display*             display;
        wl_registry*            registry;
    };

    static void randname(i8 *buf) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        i32 r = ts.tv_nsec;
        for (i32 i = 0; i < 6; ++i) {
            buf[i] = 'A'+(r&15)+(r&16)*2;
            r >>= 5;
        }
    }

    static i32 create_shm_file(void)
    {
        i32 retries = 100;
        do {
            char name[] = "/wl_shm-XXXXXX";
            randname(name + sizeof(name) - 7);
            --retries;
            i32 fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (fd >= 0) {
                shm_unlink(name);
                return fd;
            }
        } while (retries > 0 && errno == EEXIST);
        return -1;
    }

    static i32 allocate_shm_file(usize size)
    {
        i32 fd = create_shm_file();
        if (fd < 0)
            return -1;
        i32 ret;
        do {
            ret = ftruncate(fd, size);
        } while (ret < 0 && errno == EINTR);
        if (ret < 0) {
            close(fd);
            return -1;
        }
        return fd;
    }

    static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
    {
        /* Sent by the compositor when it's no longer using this buffer */
        wl_buffer_destroy(wl_buffer);
    }

    static const struct wl_buffer_listener buffer_listener = {
        .release = wl_buffer_release,
    };

    static wl_buffer* draw_frame(GlobalObjectsState* state) {
        i32 fd = allocate_shm_file(SHM_POOL_SIZE);
        if (fd == -1) {
            return nullptr;
        }

        uint32_t *data = (uint32_t*)mmap(nullptr, SHM_POOL_SIZE,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            close(fd);
            return nullptr;
        }

        struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, SHM_POOL_SIZE);
        struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
                WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        /* Draw checkerboxed background */
        for (i32 y = 0; y < HEIGHT; ++y) {
            for (i32 x = 0; x < WIDTH; ++x) {
                if ((x + y / 8 * 8) % 16 < 8)
                    data[y * WIDTH + x] = 0xFF666666;
                else
                    data[y * WIDTH + x] = 0xFFEEEEEE;
            }
        }

        munmap(data, SHM_POOL_SIZE);
        wl_buffer_add_listener(buffer, &buffer_listener, nullptr);
        return buffer;

        // i32 back_buffer_ind = state->shm_pool_state.active_buffer + 1 % 2;
        // wl_buffer* back_buffer = state->shm_pool_state.buffers[back_buffer_ind];
        // u8* pixels = &state->shm_pool_state.pool_data[offset];
        // std::memset(pixels, 0, HEIGHT * STRIDE);
        // wl_surface_attach(state->shm_pool_state.surface, back_buffer, 0, 0);
        // wl_surface_damage(state->shm_pool_state.surface, 0, 0, WIDTH, HEIGHT);
        // wl_surface_commit(state->shm_pool_state.surface);
        //
        // state->shm_pool_state.active_buffer = back_buffer_ind;
    }

    static void xdg_wm_base_handle_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
    {
        xdg_wm_base_pong(xdg_wm_base, serial);
    }

    static const struct xdg_wm_base_listener xdg_wm_base_listener = {
        .ping = xdg_wm_base_handle_ping,
    };

    static void xdg_surface_handle_configure(void *data,
            struct xdg_surface *xdg_surface, uint32_t serial)
    {
        // GlobalObjectsState* state = static_cast<GlobalObjectsState*>(data);
        // wl_buffer* buffer = state->shm_pool_state.buffers[state->shm_pool_state.active_buffer];
        // if (!buffer) {
        //     return;
        // }
        // xdg_surface_ack_configure(xdg_surface, serial);
        // wl_surface_attach(state->shm_pool_state.surface, buffer, 0, 0);
        // wl_surface_commit(state->shm_pool_state.surface);
        GlobalObjectsState* state = (GlobalObjectsState*)data;
        xdg_surface_ack_configure(xdg_surface, serial);

        wl_buffer* buffer = draw_frame(state);
        wl_surface_attach(state->shm_pool_state.surface, buffer, 0, 0);
        wl_surface_commit(state->shm_pool_state.surface);
    }

    static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = xdg_surface_handle_configure,
    };

    static void registry_handle_global(void* data, struct wl_registry* registry,
            u32 name, const char* interface, uint32_t version)
    {
        LOG_INFO("global interface: {}", interface);
        GlobalObjectsState* state = static_cast<GlobalObjectsState*>(data);

        // wl_compositor
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            state->compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 6
            ));
        }

        // xdg_vm_base
        if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            state->xdg_wm_base = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1
            ));
            xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, static_cast<void*>(state));
        }

        // wl_shm
        if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            state->shm = static_cast<wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, 1
            ));
        }
    }

    static void registry_handle_global_remove(void* data, struct wl_registry* registry,
            u32 name)
    {
        // This space deliberately left blank
    }

    // static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
    // {
        /* Sent by the compositor when it's no longer using this buffer */
    //     wl_buffer_destroy(wl_buffer);
    // }

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
        std::memset(this->internal_state, 0, sizeof(this->internal_state));

        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        state->go_state.window_name = app_name;

        state->display = wl_display_connect(nullptr);
        if (!state->display) {
            LOG_FATAL("Failed to connect to Wayland display");
            return false;
        }
        state->registry = wl_display_get_registry(state->display);
        state->listeners.registry_listener = wl_registry_listener{
            .global = registry_handle_global,
            .global_remove = registry_handle_global_remove
        };
        // state->listeners.buffer_listener = wl_buffer_listener{ .release = buffer_handle_release };

        wl_registry_add_listener(state->registry, &state->listeners.registry_listener, &state->go_state);
        wl_display_roundtrip(state->display);

        // init surface
        state->go_state.shm_pool_state.surface = wl_compositor_create_surface(state->go_state.compositor);
        state->go_state.shm_pool_state.xdg_surface = xdg_wm_base_get_xdg_surface(state->go_state.xdg_wm_base, state->go_state.shm_pool_state.surface);
        xdg_surface_add_listener(state->go_state.shm_pool_state.xdg_surface, &xdg_surface_listener, &state->go_state);
        state->go_state.shm_pool_state.xdg_toplevel = xdg_surface_get_toplevel(state->go_state.shm_pool_state.xdg_surface);
        xdg_toplevel_set_title(state->go_state.shm_pool_state.xdg_toplevel, state->go_state.window_name);
        wl_surface_commit(state->go_state.shm_pool_state.surface);

        // init buffers
        // i32 fd = allocate_shm_file(SHM_POOL_SIZE);
        // state->go_state.shm_pool_state.pool_data = static_cast<u8*>(
        //     mmap(nullptr, SHM_POOL_SIZE,
        //     PROT_READ | PROT_WRITE, MAP_SHARED,
        //     fd, 0
        // ));
        //
        // wl_shm_pool* pool = wl_shm_create_pool(state->go_state.shm, fd, SHM_POOL_SIZE);
        //
        // for (i32 i = 0; i < 2; ++i) {
        //     const i32 offset = i * HEIGHT * STRIDE;
        //     state->go_state.shm_pool_state.buffers[i] = wl_shm_pool_create_buffer(pool, offset,
        //         WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888);
        // }
        //
        // wl_shm_pool_destroy(pool);
        // close(fd);

        return true;
    }

    PlatformState::~PlatformState() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        if (state) {
            munmap(state->go_state.shm_pool_state.pool_data, SHM_POOL_SIZE);
            for (i32 i = 0; i < 2; i++) {
                wl_buffer_destroy(state->go_state.shm_pool_state.buffers[i]);
            }
            wl_display_disconnect(state->display);
            state->display = nullptr;
            free(state);
            state = nullptr;
        }
    }

    bool PlatformState::start_event_loop() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        while (wl_display_dispatch(state->display) != -1) {}

        return true;
    }

    void            platform_console_write(std::string_view message, u8 color);
    void            platform_console_write_error(std::string_view message, u8 color);
    f64             platform_get_abs_time();
    void            platform_sleep(u64 ms);
}
#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
