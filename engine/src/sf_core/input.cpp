#include "sf_platform/defines.hpp"
#include "sf_core/clock.hpp"
#include "sf_core/input.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/event.hpp"

namespace sf {

#ifdef SF_PLATFORM_WAYLAND
static constexpr f32 KEY_REPEAT_RATE{2.5f};
static constexpr f32 KEY_REPEAT_DELAY{200.0f};

struct KeyRepeatGlobalState {
    f32     rate{KEY_REPEAT_RATE};
    f32     delay{KEY_REPEAT_DELAY};
};

struct KeyRepeatTimer {
    Clock   clock;
    bool    passed_delay;
};
#endif

// down = true, up = false
struct KeyboardState {
    bool keys[static_cast<u64>(Key::COUNT)];
};

struct MouseState {
    MousePos pos;
    // down = true, up = false
    bool buttons[static_cast<u8>(MouseButton::COUNT)];
};

struct InputState {
    MouseState      mouse_curr;
    MouseState      mouse_prev;
    KeyboardState   kb_curr;
    KeyboardState   kb_prev;
    MouseDelta      mouse_delta;
#ifdef SF_PLATFORM_WAYLAND
    KeyRepeatGlobalState key_repeat_state;
    KeyRepeatTimer       key_repeat_timers[static_cast<u8>(Key::COUNT)];
#endif
};

static InputState state;

void input_update(f64 delta_time) {
    sf_mem_copy(&state.kb_prev, &state.kb_curr, sizeof(KeyboardState));
    sf_mem_copy(&state.mouse_prev, &state.mouse_curr, sizeof(MouseState));

// repeat key event for wayland
#ifdef SF_PLATFORM_WAYLAND
    for (u8 i{0}; i < static_cast<u8>(Key::COUNT); ++i) {
        if (state.kb_prev.keys[i]) {
            KeyRepeatTimer& timer = state.key_repeat_timers[i];
            if (!timer.clock.is_running()) {
                timer.clock.start();
                continue;
            }

            timer.clock.update_and_get_delta();

            if (!timer.passed_delay) {
                if (timer.clock.elapsed_time * 1000 > state.key_repeat_state.delay) {
                    timer.passed_delay = true;
                    timer.clock.restart();
                }
            } else {
                if (timer.clock.elapsed_time * 1000 > state.key_repeat_state.rate) {
                    input_process_key(static_cast<Key>(i), true);
                    timer.clock.restart();
                }
            }
       } else {
            KeyRepeatTimer& timer = state.key_repeat_timers[i];
            if (timer.clock.is_running()) {
                timer.clock.stop();
                timer.passed_delay = false;
            }
       }
    }
#endif
}

// keyboard input
bool input_is_key_down(Key key) {
    return state.kb_curr.keys[static_cast<u8>(key)];
}

bool input_is_key_up(Key key) {
    return !state.kb_curr.keys[static_cast<u8>(key)];
}

bool input_was_key_down(Key key) {
    return state.kb_prev.keys[static_cast<u8>(key)];
}

bool input_was_key_up(Key key) {
    return !state.kb_prev.keys[static_cast<u8>(key)];
}

void input_process_key(Key key, bool is_pressed) {
    u8 ind{ static_cast<u8>(key) };

    state.kb_curr.keys[ind] = is_pressed;
    EventContext context;
    context.data.u8[0] = ind;
    event_execute_callbacks(static_cast<u8>(is_pressed ? SystemEventCode::KEY_PRESSED : SystemEventCode::KEY_RELEASED), nullptr, {context});
}

// mouse input
bool input_is_button_down(MouseButton button) {
    return state.mouse_curr.buttons[static_cast<u8>(button)];
}

bool input_is_button_up(MouseButton button) {
    return !state.mouse_curr.buttons[static_cast<u8>(button)];
}

bool input_was_button_down(MouseButton button) {
    return state.mouse_prev.buttons[static_cast<u8>(button)];
}

bool input_was_button_up(MouseButton button) {
    return !state.mouse_prev.buttons[static_cast<u8>(button)];
}

MousePos input_get_mouse_position() {
    return state.mouse_curr.pos;
}

MousePos input_get_previous_mouse_position() {
    return state.mouse_prev.pos;
}

void input_process_mouse_button(MouseButton button, bool is_pressed) {
    if (state.mouse_curr.buttons[static_cast<u8>(button)] != is_pressed) {
        state.mouse_curr.buttons[static_cast<u8>(button)] = is_pressed;
        EventContext context;
        context.data.u8[0] = static_cast<u8>(button);
        event_execute_callbacks(static_cast<u8>(is_pressed ? SystemEventCode::MOUSE_BUTTON_PRESSED : SystemEventCode::MOUSE_BUTTON_RELEASED), nullptr, {context});
    }
}

void input_process_mouse_move(MousePos pos) {
    if (pos != state.mouse_curr.pos) {
        static bool is_first_event{true};
        state.mouse_curr.pos = pos;
        if (is_first_event) {
            is_first_event = false;
            return;
        }

        state.mouse_delta.x = state.mouse_curr.pos.x - state.mouse_prev.pos.x;
        // invert
        state.mouse_delta.y = state.mouse_prev.pos.y - state.mouse_curr.pos.y;

        EventContext context;
        context.data.f32[0] = state.mouse_delta.x;
        context.data.f32[1] = state.mouse_delta.y;
        event_execute_callbacks(static_cast<u8>(SystemEventCode::MOUSE_MOVED), nullptr, {context});
    }
}

void input_process_mouse_wheel(i8 z_delta) {
    EventContext context;
    context.data.i8[0] = z_delta;
    event_execute_callbacks(static_cast<u8>(SystemEventCode::MOUSE_WHEEL), nullptr, {context});
}

} // sf
