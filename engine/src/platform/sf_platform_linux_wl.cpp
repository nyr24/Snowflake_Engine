#include "platform/sf_platform.hpp"
#include <ctime>

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
#include "core/sf_logger.hpp"
#include <cstring>
#include <wayland-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace sf_platform {
    // TODO: https://wayland-book.com/surfaces/shared-memory.html#creating-buffers-from-a-pool
    constexpr i32 WIDTH = 720, HEIGHT = 480;
    constexpr i32 STRIDE = WIDTH * 4;
    constexpr i32 BUFF_COUNT = 2;
    constexpr i32 SHM_POOL_SIZE = HEIGHT * STRIDE * BUFF_COUNT;

    static void randname(i8* buff, u32 len);
    static i32 create_shm_file(void);
    static i32 allocate_shm_file(size_t size);

    struct ShmPoolState {
        i32             fd;
        u8*             pool_data;
        wl_shm_pool*    pool;
    };

    struct GlobalObjectsState {
        wl_compositor*  compositor;
        wl_shm*         shm;
        ShmPoolState    shm_pool_state;
    };

    struct WaylandInternState {
        wl_display*             display;
        wl_registry*            registry;
        wl_registry_listener    registry_listener;
        GlobalObjectsState      go_state;
    };

    PlatformState PlatformState::init() {
        return PlatformState{ .internal_state = nullptr };
    }

    static void registry_handle_global(void* data, struct wl_registry* registry,
            u32 name, const char* interface, uint32_t version)
    {
        // TODO: temp
        LOG_INFO("interface: '{}', version: {}, name: {}",
                interface, version, name);

        GlobalObjectsState* state = static_cast<GlobalObjectsState*>(data);

        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            state->compositor = static_cast<wl_compositor*>(wl_registry_bind(
                    registry, name, &wl_compositor_interface, 6
            ));
        }

        if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            state->shm = static_cast<wl_shm*>(wl_registry_bind(
                    registry, name, &wl_shm_interface, 1
            ));

            state->shm_pool_state.fd = allocate_shm_file(SHM_POOL_SIZE);
            state->shm_pool_state.pool_data = static_cast<u8*>(
                    mmap(nullptr, SHM_POOL_SIZE,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    state->shm_pool_state.fd, 0
            ));
            state->shm_pool_state.pool = wl_shm_create_pool(state->shm, state->shm_pool_state.fd, SHM_POOL_SIZE);
        }
    }

    static void registry_handle_global_remove(void* data, struct wl_registry* registry,
            u32 name)
    {
        // This space deliberately left blank
    }

    // shm
    static void randname(i8* buff, u32 len) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        i32 r = ts.tv_nsec;
        for (u32 i = 0; i < len; ++i) {
            buff[i] = 'A' + (r&15) + (r&16) * 2;
            r >>= 5;
        }
    }

    static i32 create_shm_file(void)
    {
        i32 retries = 100;
        do {
            i8 name[] = "/wl_shm-XXXXXX";
            randname(name + sizeof(name) - 7, 6);
            --retries;
            i32 fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (fd >= 0) {
                shm_unlink(name);
                return fd;
            }
        } while (retries > 0 && errno == EEXIST);
        return -1;
    }

    static i32 allocate_shm_file(size_t size)
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
        state->registry = wl_display_get_registry(state->display);
        state->registry_listener = wl_registry_listener{
            .global = registry_handle_global,
            .global_remove = registry_handle_global_remove
        };

        wl_registry_add_listener(state->registry, &state->registry_listener, &state->go_state);
        wl_display_roundtrip(state->display);

        return true;
    }

    PlatformState::~PlatformState() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        if (state && state->display) {
            wl_display_disconnect(state->display);
            state->display = nullptr;
            free(state);
            state = nullptr;
        }
    }

    bool PlatformState::start_event_loop() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        while (wl_display_dispatch(state->display) != -1) {
        }

        return true;
    }

    void            platform_console_write(std::string_view message, u8 color);
    void            platform_console_write_error(std::string_view message, u8 color);
    f64             platform_get_abs_time();
    void            platform_sleep(u64 ms);
}
#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
