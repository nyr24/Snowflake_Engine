#include "sf_platform/defines.hpp"
#include <cmath>

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)

#include "sf_platform/wl_relative_pointer.hpp"
#include "sf_platform/wl_relative_pointer_glue.hpp"
#include "sf_platform/wl_pointer_constraints.hpp"
#include "sf_platform/wl_pointer_constraints_glue.hpp"
#include "sf_platform/wayland-util-custom.hpp"
#include "sf_platform/xdg-shell-protocol.hpp"

#include "sf_containers/optional.hpp"
#include "sf_core/event.hpp"
#include "sf_platform/platform.hpp"
#include "sf_core/logger.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/input.hpp"
#include "sf_core/application.hpp"
#include "sf_core/logger.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_vulkan/renderer.hpp"
#include "sf_vulkan/renderer.hpp"
#include <cstdlib>
#include <new>
#include <wayland-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <cstring>
#include <ctime>
#include <cassert>
#include <format>
#include <array>
#include <string_view>
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

namespace sf {
enum struct ColorXRGB : u32 {
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

enum struct PointerEventMask : u32 {
    POINTER_EVENT_ENTER = 1 << 0,
    POINTER_EVENT_LEAVE = 1 << 1,
    POINTER_EVENT_MOTION = 1 << 2,
    POINTER_EVENT_BUTTON = 1 << 3,
    POINTER_EVENT_AXIS = 1 << 4,
    POINTER_EVENT_AXIS_SOURCE = 1 << 5,
    POINTER_EVENT_AXIS_STOP = 1 << 6,
    POINTER_EVENT_AXIS_VALUE120 = 1 << 7,
    POINTER_EVENT_AXIS_RELATIVE_DIR = 1 << 8,
};

struct PointerAxis {
    f64         value;
    i32         value120;
    u8          relative_dir;
    bool        valid;
};

struct PointerPos {
    f32 x;
    f32 y;  
};

struct PointerEventState {
    u32         event_mask;
    u32         button;
    u32         button_state;
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
    zwp_relative_pointer_manager_v1* relative_pointer_manager;
    zwp_relative_pointer_v1*         relative_pointer;
    zwp_locked_pointer_v1*           locked_pointer;
    zwp_pointer_constraints_v1*      pointer_constraints;
    xdg_surface*            xdg_surface;
    xdg_toplevel*           xdg_toplevel;
    xdg_wm_base*            xdg_wm_base;
    wl_buffer*              buffer;
    u32*                    pool_data;
    PointerEventState       pointer_state;
    PointerPos              pointer_pos;
    KeyboardState           keyboard_state;
};

struct Listeners {
    wl_registry_listener    registry_listener;
    wl_buffer_listener      buffer_listener;
    wl_callback_listener    callback_listener;
    wl_seat_listener        seat_listener;
    wl_pointer_listener     pointer_listener;
    wl_keyboard_listener    keyboard_listener;
    xdg_wm_base_listener    xdg_wm_base_listener;
    xdg_surface_listener    xdg_surface_listener;
    xdg_toplevel_listener   xdg_toplevel_listener;
    zwp_relative_pointer_v1_listener relative_pointer_listener;
    zwp_locked_pointer_v1_listener   locked_pointer_listener;
};

struct WindowProps {
    const i8*   name;
    u32         width;
    u32         height;
    u32         stride;
    u32         shm_pool_size;
};

struct WaylandInternState {
    wl_display*             display;
    wl_registry*            registry;
    Listeners               listeners;
    GlobalObjectsState      go_state;
    WindowProps             window_props;
    ApplicationState*       app_state{nullptr};
};

static Key translate_keycode(u32 x_keycode);
static void surface_handle_frame_done(void* data, wl_callback* cb, u32 time);

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

static void xdg_surface_handle_configure(void *data, xdg_surface* xdg_surface, u32 serial)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
}

static void xdg_toplevel_handle_close(void* data, xdg_toplevel* xdg_toplevel) {
    event_execute_callbacks(static_cast<u8>(SystemEventCode::APPLICATION_QUIT), nullptr, {None::VALUE});
}

static void xdg_toplevel_handle_configure(void* data, xdg_toplevel* xdg_toplevel, i32 width, i32 height, wl_array* states) {
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    if (width == 0 || height == 0) {
        return;
    }

    EventContext context;
    context.data.u16[0] = static_cast<u16>(width);
    context.data.u16[1] = static_cast<u16>(height);
    event_execute_callbacks(SystemEventCode::RESIZED, nullptr, {context});
}

static void xdg_toplevel_handle_configure_bounds(void* data, xdg_toplevel* xdg_toplevel, i32 width, i32 height) {
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    if (width == 0 || height == 0) {
        return;
    }
    state->window_props.width = std::min(static_cast<u32>(width), state->window_props.width);
    state->window_props.height = std::min(static_cast<u32>(height), state->window_props.height);
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

    // relative pointer
    if (std::strcmp(interface, "zwp_relative_pointer_manager_v1") == 0)
    {
        state->relative_pointer_manager = static_cast<zwp_relative_pointer_manager_v1*>(
            wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1)
        );
    }

    else if (std::strcmp(interface, "zwp_pointer_constraints_v1") == 0)
    {
        state->pointer_constraints = static_cast<zwp_pointer_constraints_v1*>(
            wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1)
        );
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
    // xdg_toplevel_add_listener()

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

// Pointer events
static void pointer_handle_enter(void* data, wl_pointer* pointer, u32 serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_ENTER);
    state->go_state.pointer_state.serial = serial;
    state->go_state.pointer_pos.x = wl_fixed_to_double(x);
    state->go_state.pointer_pos.y = wl_fixed_to_double(y);
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
    // WaylandInternState* state = static_cast<WaylandInternState*>(data);
    // state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_MOTION);
    // state->go_state.pointer_state.pos_x = wl_fixed_to_double(new_x);
    // state->go_state.pointer_state.pos_y = wl_fixed_to_double(new_y);
}

static void pointer_handle_button(void* data, wl_pointer* pointer, u32 serial, u32 timestamp_ms, u32 button, u32 button_state)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_BUTTON);
    state->go_state.pointer_state.serial = serial;
    state->go_state.pointer_state.button = button;
    state->go_state.pointer_state.button_state = button_state;
}

static void pointer_handle_axis(void* data, wl_pointer* pointer, u32 timestamp_ms, u32 axis, wl_fixed_t axis_value)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS);
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

static void relative_pointer_handle_motion(
    void* data,
    struct zwp_relative_pointer_v1* pointer,
    uint32_t time_high,
    uint32_t time_low,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t dx_unaccel,
    wl_fixed_t dy_unaccel)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    state->go_state.pointer_state.event_mask |= static_cast<u32>(PointerEventMask::POINTER_EVENT_MOTION);
    state->go_state.pointer_pos.x += wl_fixed_to_double(dx);
    state->go_state.pointer_pos.y += wl_fixed_to_double(dy);
}

static void locked_pointer_handle_locked(void* userData, struct zwp_locked_pointer_v1* lockedPointer)
{
}

static void locked_pointer_handle_unlocked(void* userData, struct zwp_locked_pointer_v1* lockedPointer)
{
}

static MouseButton map_linux_btn_codes(i32 btn) {
    switch (btn) {
        case BTN_LEFT: return MouseButton::LEFT;
        case BTN_MIDDLE: return MouseButton::MIDDLE;
        case BTN_RIGHT: return MouseButton::RIGHT;
        default: return MouseButton::COUNT;
    };
}

static void pointer_handle_frame(void* data, wl_pointer* pointer)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    PointerEventState& pointer_state = state->go_state.pointer_state;
    PointerPos& pointer_pos = state->go_state.pointer_pos;

    if (pointer_state.event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_ENTER)) {
        input_process_mouse_move({ .x = pointer_pos.x, .y = pointer_pos.y });
    }

    if (pointer_state.event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_MOTION)) {
        input_process_mouse_move({ .x = pointer_pos.x, .y = pointer_pos.y });
    }

    if (pointer_state.event_mask & static_cast<u32>(PointerEventMask::POINTER_EVENT_BUTTON)) {
        MouseButton app_btn_code = map_linux_btn_codes(pointer_state.button);
        if (app_btn_code != MouseButton::COUNT) {
            input_process_mouse_button(app_btn_code, pointer_state.button_state == WL_POINTER_BUTTON_STATE_PRESSED);
        }
    }

    // wheel event
    u32 axis_events = static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS) | static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_SOURCE);
    if (axis_events & static_cast<u32>(PointerEventMask::POINTER_EVENT_AXIS_SOURCE) && pointer_state.axis_source == WL_POINTER_AXIS_SOURCE_WHEEL) {
        f64 delta_wl = pointer_state.axis[WL_POINTER_AXIS_VERTICAL_SCROLL].value;
        if (std::abs(delta_wl) > 0.05) {
            i8 delta = pointer_state.axis[WL_POINTER_AXIS_VERTICAL_SCROLL].value > 0 ? 1 : -1;
            input_process_mouse_wheel(delta);
        }
    }

    sf_mem_zero(&pointer_state, sizeof(PointerEventState));
}

// Keyboard events
static void keyboard_handle_keymap(void* data, wl_keyboard* keyboard, u32 format, i32 fd, u32 size)
{
    SF_ASSERT(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
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
    u32 time, u32 key_code, u32 key_state
)
{
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    i8 buf[128];

    key_code += 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state->go_state.keyboard_state.xkb_state, key_code);
    xkb_keysym_get_name(sym, buf, sizeof(buf));

    Key translated_key = translate_keycode(sym);

    if (translated_key < Key::COUNT) {
        input_process_key(translated_key, key_state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
}

static void keyboard_handle_enter(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface, wl_array* keys)
{
    // This space deliberately left blank
}

static void keyboard_handle_leave(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface)
{
    // This space deliberately left blank
}

static void keyboard_handle_repeat_info(void* data, wl_keyboard* keyboard, i32 rate, i32 delay)
{
    // This space deliberately left blank
}

static void keyboard_handle_modifiers(
    void* data, wl_keyboard* keyboard, u32 serial,
    u32 depressed_mods, u32 latched_mods, u32 locked_mods, u32 group
)
{
    // This space deliberately left blank
}

static void seat_handle_name(void* data, wl_seat* seat, const i8* name) {
    // This space deliberately left blank
}

static void seat_handle_capabilities(void* data, wl_seat* seat, u32 capabilities) {
    WaylandInternState* state = static_cast<WaylandInternState*>(data);
    bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
    bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

    if (have_pointer) {
        state->go_state.pointer_dev = wl_seat_get_pointer(state->go_state.seat);
        wl_pointer_add_listener(state->go_state.pointer_dev, &state->listeners.pointer_listener, static_cast<void*>(state));

        // locked pointer setup
        state->go_state.relative_pointer =
            zwp_relative_pointer_manager_v1_get_relative_pointer(
                state->go_state.relative_pointer_manager,
                state->go_state.pointer_dev);

        zwp_relative_pointer_v1_add_listener(state->go_state.relative_pointer,
             &state->listeners.relative_pointer_listener,
             state);

        state->go_state.locked_pointer =
            zwp_pointer_constraints_v1_lock_pointer(
                state->go_state.pointer_constraints,
                state->go_state.surface,
                state->go_state.pointer_dev,
                NULL,
                ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

        zwp_locked_pointer_v1_add_listener(state->go_state.locked_pointer,
           &state->listeners.locked_pointer_listener,
           state);
    } else if (!have_pointer && state->go_state.pointer_dev != nullptr) {
        wl_pointer_release(state->go_state.pointer_dev);
        state->go_state.pointer_dev = nullptr;
    }

    if (have_keyboard) {
        state->go_state.keyboard_dev = wl_seat_get_keyboard(state->go_state.seat);
        wl_keyboard_add_listener(state->go_state.keyboard_dev, &state->listeners.keyboard_listener, static_cast<void*>(state));
    } else if (!have_keyboard && state->go_state.keyboard_dev) {
        wl_keyboard_release(state->go_state.keyboard_dev);
        state->go_state.keyboard_dev = nullptr;
    }
}

PlatformState::PlatformState()
    : internal_state{ sf_mem_alloc(sizeof(WaylandInternState), alignof(WaylandInternState)) }
{
    std::memset(internal_state, 0, sizeof(WaylandInternState));
}

PlatformState::PlatformState(PlatformState&& rhs) noexcept
    : internal_state{ rhs.internal_state }
{
    rhs.internal_state = nullptr;
}

PlatformState& PlatformState::operator=(PlatformState&& rhs) noexcept
{
    sf_mem_free(internal_state, alignof(WaylandInternState));
    internal_state = rhs.internal_state;
    rhs.internal_state = nullptr;
    return *this;
}

bool PlatformState::startup(ApplicationState& app_state) {
    WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
    state->app_state = &app_state;

    constexpr i32 buff_count = 2;
    const u32 stride = app_state.config.width * 4;

    state->window_props = WindowProps{
        .name = app_state.config.name,
        .width = app_state.config.width,
        .height = app_state.config.height,
        .stride = stride,
        .shm_pool_size = app_state.config.height * stride * buff_count,
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
    // state->listeners.callback_listener = wl_callback_listener{ .done = surface_handle_frame_done };
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
    state->listeners.xdg_toplevel_listener = xdg_toplevel_listener{
        .configure = xdg_toplevel_handle_configure,
        .close = xdg_toplevel_handle_close,
        .configure_bounds = xdg_toplevel_handle_configure_bounds
    };
    state->listeners.relative_pointer_listener = zwp_relative_pointer_v1_listener{
        .relative_motion = relative_pointer_handle_motion 
    };
    state->listeners.locked_pointer_listener = zwp_locked_pointer_v1_listener{
        .locked = locked_pointer_handle_locked,
        .unlocked = locked_pointer_handle_unlocked
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
        sf_mem_free(state, alignof(WaylandInternState));
        state = nullptr;
    }
}

bool PlatformState::poll_events(ApplicationState& application_state) {
    WaylandInternState* state = static_cast<WaylandInternState*>(this->internal_state);
    wl_display_roundtrip(state->display);
    return true;
}

void* platform_mem_alloc(u64 byte_size, u16 alignment = 0) {
    if (alignment) {
        SF_ASSERT_MSG(is_power_of_two(alignment), "alignment should be a power of two");
        void* ptr = ::operator new(byte_size, static_cast<std::align_val_t>(alignment), std::nothrow);
        if (!ptr) {
            panic("Out of memory");
        }
        return ptr;
    } else {
        void* ptr = ::operator new(byte_size, std::nothrow);
        if (!ptr) {
            panic("Out of memory");
        }
        return ptr;
    }
}

static constexpr std::array<std::string_view, static_cast<u16>(LogLevel::COUNT)> color_strings = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;28"};

void platform_console_write(char* message_buff, u16 written_count, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    char message_buff2[OUTPUT_PRINT_BUFFER_CAPACITY] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff2, OUTPUT_PRINT_BUFFER_CAPACITY, "\033[{}m{}\033[0m", color_strings[color], const_cast<const char*>(message_buff));
    std::cout << std::string_view(const_cast<const char*>(message_buff2), res.out);
}

void platform_console_write_error(char* message_buff, u16 written_count, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    char message_buff2[OUTPUT_PRINT_BUFFER_CAPACITY] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff2, OUTPUT_PRINT_BUFFER_CAPACITY, "\033[{}m{}\033[0m", color_strings[color], const_cast<const char*>(message_buff));
    std::cerr << std::string_view(const_cast<const char*>(message_buff2), res.out);
}

f64 platform_get_abs_time() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 0.000000001;
}

void platform_sleep(u64 ms) {
#if _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000 * 1000;
    nanosleep(&ts, 0);
#else
    if (ms >= 1000) {
        sleep(ms / 1000);
    }
    usleep((ms % 1000) * 1000);
#endif
}

u32 platform_get_mem_page_size() {
    return static_cast<u32>(sysconf(_SC_PAGESIZE));
}

void platform_get_required_extensions(FixedArray<const char*, REQUIRED_EXTENSION_CAPACITY>& required_extensions) {
    required_extensions.append("VK_KHR_wayland_surface");
}

void platform_create_vk_surface(PlatformState& plat_state, VulkanContext& context) {
    WaylandInternState* state = static_cast<WaylandInternState*>(plat_state.internal_state);
    VkWaylandSurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr, 0, state->display, state->go_state.surface};
    sf_vk_check(vkCreateWaylandSurfaceKHR(context.instance, &create_info, nullptr, &context.surface));
}

Key translate_keycode(u32 x_keycode) {
    switch (x_keycode) {
        case XKB_KEY_BackSpace:
            return Key::BACKSPACE;
        case XKB_KEY_Return:
            return Key::ENTER;
        case XKB_KEY_Tab:
            return Key::TAB;
            //case XKB_KEY_Shift: return Key::SHIFT;
            //case XKB_KEY_Control: return Key::CONTROL;

        case XKB_KEY_Pause:
            return Key::PAUSE;
        case XKB_KEY_Caps_Lock:
            return Key::CAPITAL;

        case XKB_KEY_Escape:
            return Key::ESCAPE;

            // Not supported
            // case : return Key::CONVERT;
            // case : return Key::NONCONVERT;
            // case : return Key::ACCEPT;

        case XKB_KEY_Mode_switch:
            return Key::MODECHANGE;

        case XKB_KEY_space:
            return Key::SPACE;
        case XKB_KEY_Prior:
            return Key::PRIOR;
        case XKB_KEY_Next:
            return Key::NEXT;
        case XKB_KEY_End:
            return Key::END;
        case XKB_KEY_Home:
            return Key::HOME;
        case XKB_KEY_Left:
            return Key::LEFT;
        case XKB_KEY_Up:
            return Key::UP;
        case XKB_KEY_Right:
            return Key::RIGHT;
        case XKB_KEY_Down:
            return Key::DOWN;
        case XKB_KEY_Select:
            return Key::SELECT;
        case XKB_KEY_Print:
            return Key::PRINT;
        case XKB_KEY_Execute:
            return Key::EXECUTE;
        // case XKB_KEY_snapshot: return Key::SNAPSHOT; // not supported
        case XKB_KEY_Insert:
            return Key::INSERT;
        case XKB_KEY_Delete:
            return Key::DELETE;
        case XKB_KEY_Help:
            return Key::HELP;

        case XKB_KEY_Meta_L:
            return Key::LWIN;  // TODO: not sure this is right
        case XKB_KEY_Meta_R:
            return Key::RWIN;
            // case XKB_KEY_apps: return Key::APPS; // not supported

            // case XKB_KEY_sleep: return Key::SLEEP; //not supported

        case XKB_KEY_KP_0:
            return Key::NUMPAD0;
        case XKB_KEY_KP_1:
            return Key::NUMPAD1;
        case XKB_KEY_KP_2:
            return Key::NUMPAD2;
        case XKB_KEY_KP_3:
            return Key::NUMPAD3;
        case XKB_KEY_KP_4:
            return Key::NUMPAD4;
        case XKB_KEY_KP_5:
            return Key::NUMPAD5;
        case XKB_KEY_KP_6:
            return Key::NUMPAD6;
        case XKB_KEY_KP_7:
            return Key::NUMPAD7;
        case XKB_KEY_KP_8:
            return Key::NUMPAD8;
        case XKB_KEY_KP_9:
            return Key::NUMPAD9;
        case XKB_KEY_multiply:
            return Key::MULTIPLY;
        case XKB_KEY_KP_Add:
            return Key::ADD;
        case XKB_KEY_KP_Separator:
            return Key::SEPARATOR;
        case XKB_KEY_KP_Subtract:
            return Key::SUBTRACT;
        case XKB_KEY_KP_Decimal:
            return Key::DECIMAL;
        case XKB_KEY_KP_Divide:
            return Key::DIVIDE;
        case XKB_KEY_F1:
            return Key::F1;
        case XKB_KEY_F2:
            return Key::F2;
        case XKB_KEY_F3:
            return Key::F3;
        case XKB_KEY_F4:
            return Key::F4;
        case XKB_KEY_F5:
            return Key::F5;
        case XKB_KEY_F6:
            return Key::F6;
        case XKB_KEY_F7:
            return Key::F7;
        case XKB_KEY_F8:
            return Key::F8;
        case XKB_KEY_F9:
            return Key::F9;
        case XKB_KEY_F10:
            return Key::F10;
        case XKB_KEY_F11:
            return Key::F11;
        case XKB_KEY_F12:
            return Key::F12;
        case XKB_KEY_F13:
            return Key::F13;
        case XKB_KEY_F14:
            return Key::F14;
        case XKB_KEY_F15:
            return Key::F15;
        case XKB_KEY_F16:
            return Key::F16;
        case XKB_KEY_F17:
            return Key::F17;
        case XKB_KEY_F18:
            return Key::F18;
        case XKB_KEY_F19:
            return Key::F19;
        case XKB_KEY_F20:
            return Key::F20;
        case XKB_KEY_F21:
            return Key::F21;
        case XKB_KEY_F22:
            return Key::F22;
        case XKB_KEY_F23:
            return Key::F23;
        case XKB_KEY_F24:
            return Key::F24;

        case XKB_KEY_Num_Lock:
            return Key::NUMLOCK;
        case XKB_KEY_Scroll_Lock:
            return Key::SCROLL;

        case XKB_KEY_KP_Equal:
            return Key::NUMPAD_EQUAL;

        case XKB_KEY_Shift_L:
            return Key::LSHIFT;
        case XKB_KEY_Shift_R:
            return Key::RSHIFT;
        case XKB_KEY_Control_L:
            return Key::LCONTROL;
        case XKB_KEY_Control_R:
            return Key::RCONTROL;
        case XKB_KEY_Menu:
            return Key::LALT;
        // case XKB_KEY_Menu:
        //     return Key::RALT;

        case XKB_KEY_semicolon:
            return Key::SEMICOLON;
        case XKB_KEY_plus:
            return Key::PLUS;
        case XKB_KEY_comma:
            return Key::COMMA;
        case XKB_KEY_minus:
            return Key::MINUS;
        case XKB_KEY_period:
            return Key::PERIOD;
        case XKB_KEY_slash:
            return Key::SLASH;
        case XKB_KEY_grave:
            return Key::GRAVE;

        case XKB_KEY_a:
        case XKB_KEY_A:
            return Key::A;
        case XKB_KEY_b:
        case XKB_KEY_B:
            return Key::B;
        case XKB_KEY_c:
        case XKB_KEY_C:
            return Key::C;
        case XKB_KEY_d:
        case XKB_KEY_D:
            return Key::D;
        case XKB_KEY_e:
        case XKB_KEY_E:
            return Key::E;
        case XKB_KEY_f:
        case XKB_KEY_F:
            return Key::F;
        case XKB_KEY_g:
        case XKB_KEY_G:
            return Key::G;
        case XKB_KEY_h:
        case XKB_KEY_H:
            return Key::H;
        case XKB_KEY_i:
        case XKB_KEY_I:
            return Key::I;
        case XKB_KEY_j:
        case XKB_KEY_J:
            return Key::J;
        case XKB_KEY_k:
        case XKB_KEY_K:
            return Key::K;
        case XKB_KEY_l:
        case XKB_KEY_L:
            return Key::L;
        case XKB_KEY_m:
        case XKB_KEY_M:
            return Key::M;
        case XKB_KEY_n:
        case XKB_KEY_N:
            return Key::N;
        case XKB_KEY_o:
        case XKB_KEY_O:
            return Key::O;
        case XKB_KEY_p:
        case XKB_KEY_P:
            return Key::P;
        case XKB_KEY_q:
        case XKB_KEY_Q:
            return Key::Q;
        case XKB_KEY_r:
        case XKB_KEY_R:
            return Key::R;
        case XKB_KEY_s:
        case XKB_KEY_S:
            return Key::S;
        case XKB_KEY_t:
        case XKB_KEY_T:
            return Key::T;
        case XKB_KEY_u:
        case XKB_KEY_U:
            return Key::U;
        case XKB_KEY_v:
        case XKB_KEY_V:
            return Key::V;
        case XKB_KEY_w:
        case XKB_KEY_W:
            return Key::W;
        case XKB_KEY_x:
        case XKB_KEY_X:
            return Key::X;
        case XKB_KEY_y:
        case XKB_KEY_Y:
            return Key::Y;
        case XKB_KEY_z:
        case XKB_KEY_Z:
            return Key::Z;

        case XKB_KEY_0:
            return Key::_0;
        case XKB_KEY_1:
            return Key::_1;
        case XKB_KEY_2:
            return Key::_2;
        case XKB_KEY_3:
            return Key::_3;
        case XKB_KEY_4:
            return Key::_4;
        case XKB_KEY_5:
            return Key::_5;
        case XKB_KEY_6:
            return Key::_6;
        case XKB_KEY_7:
            return Key::_7;
        case XKB_KEY_8:
            return Key::_8;
        case XKB_KEY_9:
            return Key::_9;

        default: return Key::COUNT;
    }
}

}
#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_WAYLAND)
