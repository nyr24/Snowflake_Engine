#include "platform/sf_platform.hpp"

#if defined(SF_PLATFORM_LINUX) && defined(SF_PLATFORM_X11)
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <sys/time.h>
#include <xcb/xproto.h>

#if _POSIX_C_SOURCE >= 199309L
#include <time.h> // nanosleep
#else
#include <unistd.h> // usleep
#endif

#include <cstdlib>
#include <cstring>
#include "core/sf_util.hpp"

namespace sf_platform {
    struct X11InternState {
        Display*            display;
        xcb_connection_t*   connection;
        xcb_window_t        window;
        xcb_screen_t*       screen;
        xcb_atom_t          wm_protocols;
        xcb_atom_t          wm_delete_win;
    };

    PlatformState PlatformState::init() {
        auto state = PlatformState{ .internal_state = sf_alloc<X11InternState>(1, true) };
        std::memset(state.internal_state, 0, sizeof(state.internal_state));
        return state;
    }

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
            free(state);
            this->internal_state = nullptr;
        }
    }

    bool PlatformState::start_event_loop() {
        X11InternState* state = static_cast<X11InternState*>(this->internal_state);

        xcb_generic_event_t* event;
        xcb_client_message_event_t* client_msg;

        bool quit_flagged = false;

        while (true) {
            event = xcb_poll_for_event(state->connection);
            if (!event) {
                break;
            }

            switch (event->response_type & ~0x80) {
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE: {
                    //  TODO:
                } break;
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE: {
                    //  TODO:
                } break;
                case XCB_MOTION_NOTIFY:
                break;
                case XCB_CONFIGURE_NOTIFY: {
                    //  TODO:
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

            free(event);
        }

        return !quit_flagged;
    }
}

#endif
