#include "sf_platform/defines.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
#include "sf_platform/platform.hpp"
#include "sf_core/input.hpp"
#include "sf_core/utility.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/asserts_sf.hpp"
#include "sf_core/logger.hpp"
#include "sf_vulkan/types.hpp"
#include "sf_ds/array_list.hpp"
#include <iostream>
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <sys/time.h>
#include <xcb/xproto.h>
#include <time.h> // nanosleep
#include <unistd.h> // usleep
#include <cstring>

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h>

namespace sf {
struct X11InternState {
    Display*            display;
    xcb_connection_t*   connection;
    xcb_window_t        window;
    xcb_screen_t*       screen;
    xcb_atom_t          wm_protocols;
    xcb_atom_t          wm_delete_win;
};

PlatformState::PlatformState()
    : internal_state{ platform_mem_alloc(sizeof(X11InternState), alignof(X11InternState)) }
{
    std::memset(internal_state, 0, sizeof(internal_state));
}

static Key translate_keycode(u32 x_keycode);

bool PlatformState::startup(
    const char* app_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height
)
{
    X11InternState* state = static_cast<X11InternState*>(this->internal_state);
    state->display = XOpenDisplay(nullptr);
    if (!state->display) {
        LOG_FATAL("Can't open X11 display");
        return false;
    }

    XAutoRepeatOff(state->display);

    state->connection = XGetXCBConnection(state->display);

    if (xcb_connection_has_error(state->connection)) {
        LOG_FATAL("Failed to connect to X server via Xcb");
        return false;
    }

    const xcb_setup_t* setup = xcb_get_setup(state->connection);

    xcb_screen_iterator_t screen_it = xcb_setup_roots_iterator(setup);

    for (i32 s = 0; s > 0; s--) {
        xcb_screen_next(&screen_it);
    }

    state->screen = screen_it.data;
    state->window = xcb_generate_id(state->connection);

    u32 event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 event_values = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    u32 value_list[] = {state->screen->black_pixel, event_values};

    xcb_void_cookie_t cookie = xcb_create_window(
        state->connection,
        XCB_COPY_FROM_PARENT,
        state->window,
        state->screen->root,
        x,
        y,
        width,
        height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        state->screen->root_visual,
        event_mask,
        value_list
    );

    // set title
    xcb_change_property(
        state->connection,
        XCB_PROP_MODE_REPLACE,
        state->window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        strlen(app_name),
        app_name
    );

    // handle window deletion
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
        state->connection,
        0,
        strlen("WM_DELETE_WINDOW"),
        "WM_DELETE_WINDOW"
    );

    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
        state->connection,
        0,
        strlen("WM_PROTOCOLS"),
        "WM_PROTOCOLS"
    );

    xcb_intern_atom_reply_t* wm_delete_reply = xcb_intern_atom_reply(
        state->connection,
        wm_delete_cookie,
        nullptr
    );

    xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(
        state->connection,
        wm_protocols_cookie,
        nullptr
    );

    state->wm_delete_win = wm_delete_reply->atom;
    state->wm_protocols = wm_protocols_reply->atom;

    xcb_change_property(
        state->connection,
        XCB_PROP_MODE_REPLACE,
        state->window,
        wm_protocols_reply->atom,
        4,
        32,
        1,
        &wm_delete_reply->atom
    );

    i32 stream_result = xcb_flush(state->connection);
    if (stream_result <= 0) {
        LOG_FATAL("Error occurred when flushing the stream: {}", stream_result);
        return false;
    }

    return true;
}

PlatformState::~PlatformState() {
    if (this->internal_state) {
        X11InternState* state = static_cast<X11InternState*>(this->internal_state);
        XAutoRepeatOn(state->display);
        xcb_destroy_window(state->connection, state->window);
        sf_mem_free(state, sizeof(X11InternState), alignof(X11InternState));
        this->internal_state = nullptr;
    }
}

bool PlatformState::start_event_loop(ApplicationState& app_state) {
    X11InternState* state = static_cast<X11InternState*>(this->internal_state);

    xcb_generic_event_t* event;
    xcb_client_message_event_t* client_msg;

    bool quit_flagged = false;

    while ((event = xcb_poll_for_event(state->connection))) {
        if (!event) {
            break;
        }

        switch (event->response_type & ~0x80) {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: {
                // Key press event - xcb_key_press_event_t and xcb_key_release_event_t are the same
                xcb_key_press_event_t* kb_event = reinterpret_cast<xcb_key_press_event_t*>(event);
                bool pressed = event->response_type == XCB_KEY_PRESS;
                xcb_keycode_t code = kb_event->detail;
                KeySym key_sym = XkbKeycodeToKeysym(
                    state->display,
                    static_cast<KeyCode>(code),  //event.xkey.keycode,
                    0,
                    code & ShiftMask ? 1 : 0);

                Key key = translate_keycode(key_sym);
                input_process_key(key, pressed);
            } break;
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                xcb_button_press_event_t* mouse_event = reinterpret_cast<xcb_button_press_event_t*>(event);
                bool pressed = event->response_type == XCB_BUTTON_PRESS;
                MouseButton mouse_button = MouseButton::COUNT;
                switch (mouse_event->detail) {
                    case XCB_BUTTON_INDEX_1:
                        mouse_button = MouseButton::LEFT;
                        break;
                    case XCB_BUTTON_INDEX_2:
                        mouse_button = MouseButton::MIDDLE;
                        break;
                    case XCB_BUTTON_INDEX_3:
                        mouse_button = MouseButton::RIGHT;
                        break;
                }

                // Pass over to the input subsystem.
                if (mouse_button != MouseButton::COUNT) {
                    input_process_mouse_button(mouse_button, pressed);
                }
            } break;
            case XCB_MOTION_NOTIFY: {
                // Mouse move
                xcb_motion_notify_event_t* move_event = reinterpret_cast<xcb_motion_notify_event_t*>(event);
                input_process_mouse_move(MousePos{ .x = move_event->event_x, .y = move_event->event_y });
            } break;
            case XCB_CONFIGURE_NOTIFY: {
                //  TODO: resizing
            } break;
            case XCB_CLIENT_MESSAGE: {
                client_msg = reinterpret_cast<xcb_client_message_event_t*>(event);

                // window close
                if (client_msg->data.data32[0] == state->wm_delete_win) {
                    quit_flagged = true;
                }
            } break;
            default:
                break;
        }

        sf_mem_free(event, sizeof(xcb_generic_event_t));
    }

    return !quit_flagged;
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

static constexpr std::array<std::string_view, 6> color_strings = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};

void platform_console_write(const i8* message, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    // printf("\033[%sm%s\033[0m", color_strings[color], message);
    constexpr usize BUFF_LEN{ 200 };
    char message_buff[BUFF_LEN] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff, BUFF_LEN, "\033[{}m{}\033[0m", color_strings[color], message);
    std::cout << std::string_view(const_cast<const i8*>(message_buff), res.out);
}

void platform_console_write_error(const i8* message, u8 color) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    // printf("\033[%sm%s\033[0m", color_strings[color], message);
    constexpr usize BUFF_LEN{ 200 };
    char message_buff[BUFF_LEN] = {0};
    const std::format_to_n_result res = std::format_to_n(message_buff, BUFF_LEN, "\033[{}m{}\033[0m", color_strings[color], message);
    std::cerr << std::string_view(const_cast<const i8*>(message_buff), res.out);
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

void platform_get_required_extensions(FixedArrayList<const char*, 5>& required_extensions) {
    required_extensions.append("VK_KHR_xcb_surface");
}

void platform_create_vk_surface(PlatformState& plat_state, VulkanContext& context) {
    X11InternState* state = static_cast<X11InternState*>(plat_state.internal_state);

    VkXcbSurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    create_info.connection = state->connection;
    create_info.window = state->window;

    sf_vk_check(vkCreateXcbSurfaceKHR(context.instance, &create_info, &context.allocator, &context.surface));
}

Key translate_keycode(u32 x_keycode) {
    switch (x_keycode) {
        case XK_BackSpace:
            return Key::BACKSPACE;
        case XK_Return:
            return Key::ENTER;
        case XK_Tab:
            return Key::TAB;
            //case XK_Shift: return Key::SHIFT;
            //case XK_Control: return Key::CONTROL;

        case XK_Pause:
            return Key::PAUSE;
        case XK_Caps_Lock:
            return Key::CAPITAL;

        case XK_Escape:
            return Key::ESCAPE;

            // Not supported
            // case : return Key::CONVERT;
            // case : return Key::NONCONVERT;
            // case : return Key::ACCEPT;

        case XK_Mode_switch:
            return Key::MODECHANGE;

        case XK_space:
            return Key::SPACE;
        case XK_Prior:
            return Key::PRIOR;
        case XK_Next:
            return Key::NEXT;
        case XK_End:
            return Key::END;
        case XK_Home:
            return Key::HOME;
        case XK_Left:
            return Key::LEFT;
        case XK_Up:
            return Key::UP;
        case XK_Right:
            return Key::RIGHT;
        case XK_Down:
            return Key::DOWN;
        case XK_Select:
            return Key::SELECT;
        case XK_Print:
            return Key::PRINT;
        case XK_Execute:
            return Key::EXECUTE;
        // case XK_snapshot: return Key::SNAPSHOT; // not supported
        case XK_Insert:
            return Key::INSERT;
        case XK_Delete:
            return Key::DELETE;
        case XK_Help:
            return Key::HELP;

        case XK_Meta_L:
            return Key::LWIN;  // TODO: not sure this is right
        case XK_Meta_R:
            return Key::RWIN;
            // case XK_apps: return Key::APPS; // not supported

            // case XK_sleep: return Key::SLEEP; //not supported

        case XK_KP_0:
            return Key::NUMPAD0;
        case XK_KP_1:
            return Key::NUMPAD1;
        case XK_KP_2:
            return Key::NUMPAD2;
        case XK_KP_3:
            return Key::NUMPAD3;
        case XK_KP_4:
            return Key::NUMPAD4;
        case XK_KP_5:
            return Key::NUMPAD5;
        case XK_KP_6:
            return Key::NUMPAD6;
        case XK_KP_7:
            return Key::NUMPAD7;
        case XK_KP_8:
            return Key::NUMPAD8;
        case XK_KP_9:
            return Key::NUMPAD9;
        case XK_multiply:
            return Key::MULTIPLY;
        case XK_KP_Add:
            return Key::ADD;
        case XK_KP_Separator:
            return Key::SEPARATOR;
        case XK_KP_Subtract:
            return Key::SUBTRACT;
        case XK_KP_Decimal:
            return Key::DECIMAL;
        case XK_KP_Divide:
            return Key::DIVIDE;
        case XK_F1:
            return Key::F1;
        case XK_F2:
            return Key::F2;
        case XK_F3:
            return Key::F3;
        case XK_F4:
            return Key::F4;
        case XK_F5:
            return Key::F5;
        case XK_F6:
            return Key::F6;
        case XK_F7:
            return Key::F7;
        case XK_F8:
            return Key::F8;
        case XK_F9:
            return Key::F9;
        case XK_F10:
            return Key::F10;
        case XK_F11:
            return Key::F11;
        case XK_F12:
            return Key::F12;
        case XK_F13:
            return Key::F13;
        case XK_F14:
            return Key::F14;
        case XK_F15:
            return Key::F15;
        case XK_F16:
            return Key::F16;
        case XK_F17:
            return Key::F17;
        case XK_F18:
            return Key::F18;
        case XK_F19:
            return Key::F19;
        case XK_F20:
            return Key::F20;
        case XK_F21:
            return Key::F21;
        case XK_F22:
            return Key::F22;
        case XK_F23:
            return Key::F23;
        case XK_F24:
            return Key::F24;

        case XK_Num_Lock:
            return Key::NUMLOCK;
        case XK_Scroll_Lock:
            return Key::SCROLL;

        case XK_KP_Equal:
            return Key::NUMPAD_EQUAL;

        case XK_Shift_L:
            return Key::LSHIFT;
        case XK_Shift_R:
            return Key::RSHIFT;
        case XK_Control_L:
            return Key::LCONTROL;
        case XK_Control_R:
            return Key::RCONTROL;
        // case XK_Menu: return Key::LMENU;
        case XK_Menu:
            return Key::RMENU;

        case XK_semicolon:
            return Key::SEMICOLON;
        case XK_plus:
            return Key::PLUS;
        case XK_comma:
            return Key::COMMA;
        case XK_minus:
            return Key::MINUS;
        case XK_period:
            return Key::PERIOD;
        case XK_slash:
            return Key::SLASH;
        case XK_grave:
            return Key::GRAVE;

        case XK_a:
        case XK_A:
            return Key::A;
        case XK_b:
        case XK_B:
            return Key::B;
        case XK_c:
        case XK_C:
            return Key::C;
        case XK_d:
        case XK_D:
            return Key::D;
        case XK_e:
        case XK_E:
            return Key::E;
        case XK_f:
        case XK_F:
            return Key::F;
        case XK_g:
        case XK_G:
            return Key::G;
        case XK_h:
        case XK_H:
            return Key::H;
        case XK_i:
        case XK_I:
            return Key::I;
        case XK_j:
        case XK_J:
            return Key::J;
        case XK_k:
        case XK_K:
            return Key::K;
        case XK_l:
        case XK_L:
            return Key::L;
        case XK_m:
        case XK_M:
            return Key::M;
        case XK_n:
        case XK_N:
            return Key::N;
        case XK_o:
        case XK_O:
            return Key::O;
        case XK_p:
        case XK_P:
            return Key::P;
        case XK_q:
        case XK_Q:
            return Key::Q;
        case XK_r:
        case XK_R:
            return Key::R;
        case XK_s:
        case XK_S:
            return Key::S;
        case XK_t:
        case XK_T:
            return Key::T;
        case XK_u:
        case XK_U:
            return Key::U;
        case XK_v:
        case XK_V:
            return Key::V;
        case XK_w:
        case XK_W:
            return Key::W;
        case XK_x:
        case XK_X:
            return Key::X;
        case XK_y:
        case XK_Y:
            return Key::Y;
        case XK_z:
        case XK_Z:
            return Key::Z;

        case XK_0:
            return Key::_0;
        case XK_1:
            return Key::_1;
        case XK_2:
            return Key::_2;
        case XK_3:
            return Key::_3;
        case XK_4:
            return Key::_4;
        case XK_5:
            return Key::_5;
        case XK_6:
            return Key::_6;
        case XK_7:
            return Key::_7;
        case XK_8:
            return Key::_8;
        case XK_9:
            return Key::_9;

        default: return Key::COUNT;
    }
}

} // sf

#endif // defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
