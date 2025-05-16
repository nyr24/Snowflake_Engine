#include "platform/sf_platform.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
#include "platform/wayland-util-custom.hpp"
#include "core/sf_logger.hpp"
#include "platform/xdg-shell-protocol.hpp"
#include <wayland-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <cassert>

namespace sf_platform {
    enum class ColorXRGB : u32 {
        BLACK  = 0xFF333333,
        WHITE  = 0xFFFFFFFF,
        GREEN  = 0xFF00FF00,
        RED    = 0xFFFF0000,
        YELLOW = 0xFFCFB000,
        BLUE   = 0xFF0000FF,
        PURPLE = 0xFF8800C5,
    };

    using wl_surface    = struct wl_surface;
    using xdg_wm_base   = struct xdg_wm_base;
    using wl_callback   = struct wl_callback;

    struct KeyboardState {
        xkb_context*    xkb_context;
        xkb_keymap*     xkb_keymap;
        xkb_state*      xkb_state;
        u32             serial;
    };

    enum class PointerEventMask : u32 {
        POINTER_EVENT_ENTER = 1 << 0,
        POINTER_EVENT_LEAVE = 1 << 1,
        POINTER_EVENT_MOTION = 1 << 2,
        POINTER_EVENT_BUTTON = 1 << 3,
        POINTER_EVENT_AXIS = 1 << 4,
        POINTER_EVENT_AXIS_SOURCE = 1 << 5,
        POINTER_EVENT_AXIS_STOP = 1 << 6,
        POINTER_EVENT_AXIS_VALUE120 = 1 << 7,
        POINTER_EVENT_AXIS_RELATIVE_DIR = 1 << 8
    };

    struct PointerAxis {
        f64         value;
        i32         value120;
        u8          relative_dir;
        bool        valid;
    };

    struct PointerEventState {
        u32         event_mask;
        f64         surface_x;
        f64         surface_y;
        u32         button;
        u32         button_state;
        u32         time;
        u32         serial;
        u32         axis_source;
        PointerAxis axis[2];
    };

    struct GlobalObjectsState {
        wl_shm*                 shm;
        wl_compositor*          compositor;
        wl_seat*                seat;
        wl_pointer*             pointer_dev;
        wl_keyboard*            keyboard_dev;
        wl_surface*             surface;
        wl_region*              region_opaque;
        xdg_surface*            xdg_surface;
        xdg_toplevel*           xdg_toplevel;
        xdg_wm_base*            xdg_wm_base;
        wl_buffer*              buffer;
        u32*                    pool_data;
        u32                     last_frame = 0;
        PointerEventState       pointer_state;
        KeyboardState           keyboard_state;
    };

    struct Listeners {
        wl_registry_listener    registry_listener;
        wl_buffer_listener      buffer_listener;
        xdg_wm_base_listener    xdg_wm_base_listener;
        wl_callback_listener    callback_listener;
        wl_seat_listener        seat_listener;
        wl_pointer_listener     pointer_listener;
        wl_keyboard_listener    keyboard_listener;
        xdg_surface_listener    xdg_surface_listener;
    };

    struct WindowProps {
        const i8*   name;
        i32         width;
        i32         height;
        i32         stride;
        i32         shm_pool_size;
    };

    struct WaylandInternState {
        wl_display*             display;
        wl_registry*            registry;
        Listeners               listeners;
        GlobalObjectsState      go_state;
        WindowProps             window_props;
    };

    static void draw_frame(WaylandInternState* state);

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

    static void buffer_handle_release(void *data, wl_buffer *wl_buffer)
    {
        /* Sent by the compositor when it's no longer using this buffer */
        wl_buffer_destroy(wl_buffer);
    }

    static void xdg_wm_base_handle_ping(void *data, xdg_wm_base *xdg_wm_base, u32 serial)
    {
        xdg_wm_base_pong(xdg_wm_base, serial);
    }

    static void xdg_surface_handle_configure(void *data,
            struct xdg_surface *xdg_surface, u32 serial)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        xdg_surface_ack_configure(xdg_surface, serial);
        draw_frame(state);
    }

    static void registry_handle_global(void* data, struct wl_registry* registry,
        u32 name, const char* interface, u32 version)
    {
        GlobalObjectsState* state = static_cast<GlobalObjectsState*>(data);

        // wl_shm
        if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            state->shm = static_cast<wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, 1
            ));
        }

        // wl_compositor
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            state->compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 6
            ));
        }

        // wl_seat
        if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            state->seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, 9
            ));
        }

        // xdg_vm_base
        if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            state->xdg_wm_base = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1
            ));
        }
    }

    static void registry_handle_global_remove(void* data, struct wl_registry* registry,
            u32 name)
    {
        // This space deliberately left blank
    }

    static void init_surface(WaylandInternState* state) {
        state->go_state.surface = wl_compositor_create_surface(state->go_state.compositor);
        state->go_state.xdg_surface = xdg_wm_base_get_xdg_surface(state->go_state.xdg_wm_base, state->go_state.surface);
        xdg_surface_add_listener(state->go_state.xdg_surface, &state->listeners.xdg_surface_listener, state);
        xdg_wm_base_add_listener(state->go_state.xdg_wm_base, &state->listeners.xdg_wm_base_listener, &state->go_state);

        state->go_state.xdg_toplevel = xdg_surface_get_toplevel(state->go_state.xdg_surface);
        xdg_toplevel_set_title(state->go_state.xdg_toplevel, state->window_props.name);

        // making all surface as opaque and compositor dont need to worry about drawing windows behind our app
        state->go_state.region_opaque = wl_compositor_create_region(state->go_state.compositor);
        wl_region_add(state->go_state.region_opaque, 0, 0, state->window_props.width, state->window_props.height);
        wl_surface_set_opaque_region(state->go_state.surface, state->go_state.region_opaque);
        wl_region_destroy(state->go_state.region_opaque);

        wl_surface_commit(state->go_state.surface);
        wl_display_flush(state->display);
    }

    static void init_buffers(WaylandInternState* state) {
        i32 fd = allocate_shm_file(state->window_props.shm_pool_size);
        if (fd == -1) {
            LOG_ERROR("Invalid file descriptor");
            std::exit(1);
        }

        state->go_state.pool_data = static_cast<u32*>(
            mmap(nullptr, state->window_props.shm_pool_size,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, 0
        ));

        wl_shm_pool* pool = wl_shm_create_pool(state->go_state.shm, fd, state->window_props.shm_pool_size);
        state->go_state.buffer = wl_shm_pool_create_buffer(pool, 0,
            state->window_props.width, state->window_props.height, state->window_props.stride, WL_SHM_FORMAT_XRGB8888);

        wl_shm_pool_destroy(pool);
        close(fd);
    }

    static void surface_handle_frame_done(void* data, wl_callback* cb, u32 time) {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        wl_callback_destroy(cb);
        cb = wl_surface_frame(state->go_state.surface);
        wl_callback_add_listener(cb, &state->listeners.callback_listener,  static_cast<void*>(state));
        draw_frame(state);
        state->go_state.last_frame = time;
    }

    // Pointer events
    static void pointer_handle_enter(void* data, wl_pointer* pointer, u32 serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_ENTER);
        state->go_state.pointer_state.serial = serial;
        state->go_state.pointer_state.surface_x = wl_fixed_to_double(x);
        state->go_state.pointer_state.surface_y = wl_fixed_to_double(y);
        wl_pointer_set_cursor(pointer, serial, nullptr, 0, 0);
    }

    static void pointer_handle_leave(void* data, wl_pointer* pointer, u32 serial, wl_surface* surface)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_LEAVE);
        state->go_state.pointer_state.serial = serial;
    }

    static void pointer_handle_motion(void* data, wl_pointer* pointer, u32 timestamp_ms, wl_fixed_t new_x, wl_fixed_t new_y)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_MOTION);
        state->go_state.pointer_state.time = timestamp_ms;
        state->go_state.pointer_state.surface_x = wl_fixed_to_double(new_x);
        state->go_state.pointer_state.surface_y = wl_fixed_to_double(new_y);
    }

    static void pointer_handle_button(void* data, wl_pointer* pointer, u32 serial, u32 timestamp_ms, u32 button, u32 button_state)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_BUTTON);
        state->go_state.pointer_state.serial = serial;
        state->go_state.pointer_state.time = timestamp_ms;
        state->go_state.pointer_state.button = button;
        state->go_state.pointer_state.button_state = button_state;
    }

    static void pointer_handle_axis(void* data, wl_pointer* pointer, u32 timestamp_ms, u32 axis, wl_fixed_t axis_value)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS);
        state->go_state.pointer_state.time = timestamp_ms;
        state->go_state.pointer_state.axis[axis].valid = true;
        state->go_state.pointer_state.axis[axis].value = axis_value;
    }

    static void pointer_handle_axis_source(void* data, wl_pointer* pointer, u32 axis_source)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_SOURCE);
        state->go_state.pointer_state.axis_source = axis_source;
    }

    static void pointer_handle_axis_stop(void* data, wl_pointer* pointer, u32 timestamp_ms, u32 axis)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_STOP);
        state->go_state.pointer_state.time = timestamp_ms;
        state->go_state.pointer_state.axis[axis].valid = true;
    }

    static void pointer_handle_axis_value120(void* data, wl_pointer* pointer, u32 axis, i32 value120)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_VALUE120);
        state->go_state.pointer_state.axis[axis].valid = true;
        state->go_state.pointer_state.axis[axis].value120 = value120;
    }

    static void pointer_handle_axis_relative_dir(void* data, wl_pointer* pointer, u32 axis, u32 axis_rel_dir)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_RELATIVE_DIR);
        state->go_state.pointer_state.axis[axis].valid = true;
        state->go_state.pointer_state.axis[axis].relative_dir = static_cast<u8>(axis_rel_dir);
    }

    static void pointer_handle_frame(void* data, wl_pointer* pointer)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        PointerEventState* pointer_state = &state->go_state.pointer_state;

        LOG_INFO("pointer frame {}: ", pointer_state->time);

        if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_ENTER)) {
            LOG_INFO("entered x: {} y: {} ", pointer_state->surface_x, pointer_state->surface_y);
        }

        if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_LEAVE)) {
            LOG_INFO("leave ");
        }

        if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_MOTION)) {
            LOG_INFO("moved x: {} y: {} ", pointer_state->surface_x, pointer_state->surface_y);
        }

        if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_BUTTON)) {
            LOG_INFO("button: {} state: {} ", pointer_state->button, pointer_state->button_state);
        }

        u32 axis_events = static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS)
                | static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_SOURCE)
                | static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_STOP)
                | static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_VALUE120)
                | static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_RELATIVE_DIR);

        const i8* axis_name[2] = {
            "vertical",
            "horizontal",
        };

        const i8* axis_relative_dir[2] = {
            "identical",
            "inverted",
        };

        const i8* axis_source[4] = {
            "wheel",
            "finger",
            "continuous",
            "wheel tilt",
        };

        if (pointer_state->event_mask & axis_events) {
            for (size_t i = 0; i < 2; ++i) {
                if (!pointer_state->axis[i].valid) {
                    continue;
                }
                LOG_INFO("{} axis ", axis_name[i]);
                if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS)) {
                    LOG_INFO("value {} ", pointer_state->axis[i].value);
                }
                if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_SOURCE)) {
                    LOG_INFO("source {} ", axis_source[pointer_state->axis_source]);
                }
                if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_VALUE120)) {
                    LOG_INFO("value120 {} ", pointer_state->axis[i].value120);
                }
                if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_RELATIVE_DIR)) {
                    LOG_INFO("relative dir {} ", axis_relative_dir[pointer_state->axis[i].relative_dir]);
                }
                if (pointer_state->event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_STOP)) {
                    LOG_INFO("stopped ");
                }
            }
        }

        std::memset(pointer_state, 0, sizeof(*pointer_state));
    }

    // Keyboard events
    static void keyboard_handle_keymap(void* data, wl_keyboard* keyboard, u32 format, i32 fd, u32 size)
    {
        assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
        WaylandInternState* state = static_cast<WaylandInternState*>(data);

        i8* map_shm = static_cast<i8*>(
            mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0)
        );

        xkb_keymap* xkb_keymap = xkb_keymap_new_from_string(
            state->go_state.keyboard_state.xkb_context,
            map_shm,
            XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS
        );

        munmap(map_shm, size);
        close(fd);

        xkb_state* xkb_state = xkb_state_new(xkb_keymap);
        xkb_keymap_unref(state->go_state.keyboard_state.xkb_keymap);
        xkb_state_unref(state->go_state.keyboard_state.xkb_state);

        state->go_state.keyboard_state.xkb_keymap = xkb_keymap;
        state->go_state.keyboard_state.xkb_state = xkb_state;
    }

    // The key_code from this event is the Linux evdev scancode.
    // To translate this to an XKB scancode, you must add 8 to the evdev scancode.
    static void keyboard_handle_key(
        void* data, wl_keyboard* keyboard, u32 serial,
        u32 time, u32 key_code, u32 key_state)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        i8 buf[128];

        u32 keycode = key_code + 8;
        xkb_keysym_t sym = xkb_state_key_get_one_sym(
            state->go_state.keyboard_state.xkb_state, keycode);
        xkb_keysym_get_name(sym, buf, sizeof(buf));
        std::string_view key_state_sv = key_state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release";
        LOG_INFO("key: {} scan_code: {} state: {}", buf, sym, key_state_sv);

        // TODO: switch on keys
        // XKB_KEY_Alt_L

        xkb_state_key_get_utf8(state->go_state.keyboard_state.xkb_state, keycode,
            buf, sizeof(buf)
        );
        LOG_INFO("utf8: {}", buf);
    }

    static void keyboard_handle_enter(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface, wl_array* keys)
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        u32* key;

        LOG_INFO("Keyboard enter, pressed keys are:");
        wl_array_for_each(key, keys, u32*) {
            i8 buf[128];
            xkb_keysym_t sym = xkb_state_key_get_one_sym(
                state->go_state.keyboard_state.xkb_state, *key + 8
            );
            xkb_keysym_get_name(sym, buf, sizeof(buf));
            LOG_INFO("key: {} scan_code: {}", buf, sym);

            // TODO: switch on keys
            // XKB_KEY_Alt_L
 
            xkb_state_key_get_utf8(state->go_state.keyboard_state.xkb_state,
                *key + 8, buf, sizeof(buf));
            LOG_INFO("utf8: {}", buf);
        }
    }

    static void keyboard_handle_leave(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface)
    {
        // This space deliberately left blank
    }

    static void keyboard_handle_repeat_info(void* data, wl_keyboard* keyboard, i32 rate, i32 delay)
    {
        LOG_INFO("repeat rate: {} repeat delay: {}", rate, delay);
    }

    static void keyboard_handle_modifiers(
        void* data, wl_keyboard* keyboard, u32 serial,
        u32 depressed_mods, u32 latched_mods, u32 locked_mods, u32 group
    )
    {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        xkb_state_update_mask(state->go_state.keyboard_state.xkb_state, depressed_mods, latched_mods, locked_mods, 0, 0, group);
    }

    static void seat_handle_name(void* data, wl_seat* seat, const i8* name) {
        // This space deliberately left blank
    }

    static void seat_handle_capabilities(void* data, wl_seat* seat, u32 capabilities) {
        WaylandInternState* state = static_cast<WaylandInternState*>(data);
        bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
        bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

        if (have_pointer) {
            LOG_INFO("pointer device detected");
            state->go_state.pointer_dev = wl_seat_get_pointer(state->go_state.seat);
            wl_pointer_add_listener(state->go_state.pointer_dev, &state->listeners.pointer_listener, static_cast<void*>(state));
        } else if (!have_pointer && state->go_state.pointer_dev != nullptr) {
            wl_pointer_release(state->go_state.pointer_dev);
            state->go_state.pointer_dev = nullptr;
        }

        if (have_keyboard) {
            LOG_INFO("keyboard device detected");
            state->go_state.keyboard_dev = wl_seat_get_keyboard(state->go_state.seat);
            wl_keyboard_add_listener(state->go_state.keyboard_dev, &state->listeners.keyboard_listener, static_cast<void*>(state));
        } else if (!have_keyboard && state->go_state.keyboard_dev) {
            wl_keyboard_release(state->go_state.keyboard_dev);
            state->go_state.keyboard_dev = nullptr;
        }
    }

    static void draw_frame(WaylandInternState* state) {
        const i32 quot_height = state->window_props.height / 4;
        ColorXRGB colors[] = { ColorXRGB::PURPLE, ColorXRGB::GREEN, ColorXRGB::RED, ColorXRGB::YELLOW };

        for (i32 height_offset = 0; height_offset < 4; ++height_offset) {
            for (i32 y = (height_offset * quot_height); y < ((height_offset + 1) * quot_height); ++y) {
                for (i32 x = 0; x < state->window_props.width; ++x) {
                    i32 px_curr = y * (state->window_props.width) + x;
                    state->go_state.pool_data[px_curr] = static_cast<u32>(colors[height_offset]);
                }
            }
        }

        wl_surface_attach(state->go_state.surface, state->go_state.buffer, 0, 0);
        wl_surface_damage_buffer(state->go_state.surface, 0, 0, state->window_props.width, state->window_props.height);
        wl_surface_commit(state->go_state.surface);
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

        constexpr i32 buff_count = 2;
        const i32 stride = width * 4;
        state->window_props = WindowProps{
            .name = app_name,
            .width = width,
            .height = height,
            .stride = stride,
            .shm_pool_size = height * stride * buff_count,
        };

        state->display = wl_display_connect(nullptr);
        if (!state->display) {
            LOG_FATAL("Failed to connect to Wayland display");
            return false;
        }
        state->registry = wl_display_get_registry(state->display);
        state->go_state.keyboard_state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

        state->listeners.registry_listener = wl_registry_listener{
            .global = registry_handle_global,
            .global_remove = registry_handle_global_remove,
        };
        state->listeners.buffer_listener = wl_buffer_listener{ .release = buffer_handle_release };
        state->listeners.xdg_wm_base_listener = xdg_wm_base_listener{
            .ping = xdg_wm_base_handle_ping,
        };
        state->listeners.callback_listener = wl_callback_listener{ .done = surface_handle_frame_done };
        state->listeners.seat_listener = wl_seat_listener{
            .capabilities = seat_handle_capabilities,
            .name = seat_handle_name,
        };
        state->listeners.pointer_listener = wl_pointer_listener{
            .enter = pointer_handle_enter,
            .leave = pointer_handle_leave,
            .motion = pointer_handle_motion,
            .button = pointer_handle_button,
            .axis = pointer_handle_axis,
            .frame = pointer_handle_frame,
            .axis_source = pointer_handle_axis_source,
            .axis_stop = pointer_handle_axis_stop,
            .axis_value120 = pointer_handle_axis_value120,
            .axis_relative_direction = pointer_handle_axis_relative_dir
        };
        state->listeners.keyboard_listener = wl_keyboard_listener{
            .keymap = keyboard_handle_keymap,
            .enter = keyboard_handle_enter,
            .leave = keyboard_handle_leave,
            .key = keyboard_handle_key,
            .modifiers = keyboard_handle_modifiers,
            .repeat_info = keyboard_handle_repeat_info
        };
        state->listeners.xdg_surface_listener = xdg_surface_listener{
            .configure = xdg_surface_handle_configure,
        };

        wl_registry_add_listener(state->registry, &state->listeners.registry_listener, &state->go_state);
        // waits until we bind to all globals we need
        wl_display_roundtrip(state->display);

        wl_seat_add_listener(state->go_state.seat, &state->listeners.seat_listener, state);

        init_surface(state);
        init_buffers(state);

        return true;
    }

    PlatformState::~PlatformState() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        if (state) {
            if (state->go_state.pointer_dev) {
                wl_pointer_release(state->go_state.pointer_dev);
                state->go_state.pointer_dev = nullptr;
            }
            if (state->go_state.keyboard_dev) {
                wl_keyboard_release(state->go_state.keyboard_dev);
                state->go_state.keyboard_dev = nullptr;
            }
            munmap(state->go_state.pool_data, state->window_props.shm_pool_size);
            wl_display_disconnect(state->display);
            state->display = nullptr;
            free(state);
            state = nullptr;
        }
    }

    bool PlatformState::start_event_loop() {
        WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
        wl_callback* cb = wl_surface_frame(state->go_state.surface);
        wl_callback_add_listener(cb, &state->listeners.callback_listener, state);

        while (wl_display_dispatch(state->display) != -1) {}

        return true;
    }

    void            platform_console_write(std::string_view message, u8 color);
    void            platform_console_write_error(std::string_view message, u8 color);
    f64             platform_get_abs_time();
    void            platform_sleep(u64 ms);
}
#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
