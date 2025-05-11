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

    struct GlobalObjectsState {
        u32*                pool_data;
        wl_buffer*          buffers[2];
        wl_surface*         surface;
        xdg_surface*        xdg_surface;
        xdg_toplevel*       xdg_toplevel;
        wl_compositor*      compositor;
        wl_shm*             shm;
        xdg_wm_base*        xdg_wm_base;
        const i8*           window_name;
        u8                  active_buffer_ind = 0;
    };

    struct WaylandInternState {
        wl_display*             display;
        wl_registry*            registry;
        wl_registry_listener    registry_listener;
        wl_buffer_listener      buffer_listener;
        xdg_wm_base_listener    xdg_wm_base_listener;
        GlobalObjectsState      go_state;
    };

    static void draw_frame(GlobalObjectsState* state);

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

    static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
    {
        /* Sent by the compositor when it's no longer using this buffer */
        wl_buffer_destroy(wl_buffer);
    }

    static void xdg_wm_base_handle_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
    {
        xdg_wm_base_pong(xdg_wm_base, serial);
    }

    static void xdg_surface_handle_configure(void *data,
            struct xdg_surface *xdg_surface, uint32_t serial)
    {
        GlobalObjectsState* state = static_cast<GlobalObjectsState*>(data);
        xdg_surface_ack_configure(xdg_surface, serial);
        draw_frame(state);
    }

    static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = xdg_surface_handle_configure,
    };

    static void registry_handle_global(void* data, struct wl_registry* registry,
            u32 name, const char* interface, uint32_t version)
    {
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

    static void init_surface(WaylandInternState* state) {
        // init surface
        state->go_state.surface = wl_compositor_create_surface(state->go_state.compositor);
        state->go_state.xdg_surface = xdg_wm_base_get_xdg_surface(state->go_state.xdg_wm_base, state->go_state.surface);
        xdg_surface_add_listener(state->go_state.xdg_surface, &xdg_surface_listener, &state->go_state);
        xdg_wm_base_add_listener(state->go_state.xdg_wm_base, &state->xdg_wm_base_listener, &state->go_state);

        state->go_state.xdg_toplevel = xdg_surface_get_toplevel(state->go_state.xdg_surface);
        xdg_toplevel_set_title(state->go_state.xdg_toplevel, state->go_state.window_name);
        wl_surface_commit(state->go_state.surface);

    }

    static void init_buffers(WaylandInternState* state) {
        // init buffers
        i32 fd = allocate_shm_file(SHM_POOL_SIZE);
        if (fd == -1) {
            LOG_ERROR("Invalid file descriptor");
            std::exit(1);
        }

        state->go_state.pool_data = static_cast<u32*>(
            mmap(nullptr, SHM_POOL_SIZE,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, 0
        ));

        wl_shm_pool* pool = wl_shm_create_pool(state->go_state.shm, fd, SHM_POOL_SIZE);

        for (i32 i = 0; i < 2; ++i) {
            const i32 offset = i * HEIGHT * STRIDE;
            state->go_state.buffers[i] = wl_shm_pool_create_buffer(pool, offset,
                WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888);
        }

        wl_shm_pool_destroy(pool);
        close(fd);
    }

    static void draw_frame(GlobalObjectsState* state) {
#ifdef SF_DEBUG
        static u64 frame_count = 0;
        LOG_DEBUG("frame: {}", frame_count);
        frame_count++;
#endif

        static bool flag = true;
        u32 black_color =   0xFF333333;
        u32 green_color =   0xFF00FF00;
        u32 red_color   =   0xFFFF0000;
        u32 blue_color  =   0xFF0000FF;

        i32 back_buffer_ind = (state->active_buffer_ind + 1) % 2;
        wl_buffer* back_buffer = state->buffers[back_buffer_ind];
        u32 back_buffer_data_offset = back_buffer_ind * HEIGHT * STRIDE;
        u32* back_buffer_data = reinterpret_cast<u32*>( (reinterpret_cast<u8*>(state->pool_data) + back_buffer_data_offset) );

        for (i32 y = 0; y < HEIGHT; ++y) {
            for (i32 x = 0; x < WIDTH; ++x) {
                if ((x + y / 8 * 8) % 16 < 8) {
                    u32 color = black_color;
                    if (flag) {
                        color = green_color;
                    }
                    back_buffer_data[y * WIDTH + x] = color;
                }
                else {
                    u32 color = red_color;
                    if (flag) {
                        color = blue_color;
                    }
                    back_buffer_data[y * WIDTH + x] = color;
                }
            }
        }

        flag = !flag;

        wl_surface_attach(state->surface, back_buffer, 0, 0);
        wl_surface_damage(state->surface, 0, 0, WIDTH, HEIGHT);
        wl_surface_commit(state->surface);

        // swap buffers
        state->active_buffer_ind = back_buffer_ind;
    }

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
        state->registry_listener = wl_registry_listener{
            .global = registry_handle_global,
            .global_remove = registry_handle_global_remove
        };
        state->buffer_listener = wl_buffer_listener{ .release = buffer_handle_release };
        state->xdg_wm_base_listener = xdg_wm_base_listener{
            .ping = xdg_wm_base_handle_ping,
        };

        wl_registry_add_listener(state->registry, &state->registry_listener, &state->go_state);
        // waits until we bind to all globals we need
        wl_display_roundtrip(state->display);

        init_surface(state);
        init_buffers(state);

        return true;
    }

    PlatformState::~PlatformState() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        if (state) {
            munmap(state->go_state.pool_data, SHM_POOL_SIZE);
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
