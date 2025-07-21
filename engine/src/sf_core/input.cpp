#include "sf_core/input.hpp"
#include "sf_core/memory_sf.hpp"
#include "sf_core/event.hpp"

namespace sf {

// down = true, up = false
struct KeyboardState {
    bool keys[0xff];
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
};

static InputState state;

void input_update(f64 delta_time) {
    sf_mem_copy(&state.kb_curr, &state.kb_prev, sizeof(KeyboardState));
    sf_mem_copy(&state.mouse_curr, &state.mouse_prev, sizeof(MouseState));
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
    if (state.kb_curr.keys[static_cast<u8>(key)] != is_pressed) {
        state.kb_curr.keys[static_cast<u8>(key)] = is_pressed;
        EventContext context;
        context.data.u8[0] = static_cast<u8>(key);
        event_execute_callback(static_cast<u8>(is_pressed ? SystemEventCode::KEY_PRESSED : SystemEventCode::KEY_RELEASED), nullptr, &context);
    }
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
        event_execute_callback(static_cast<u8>(is_pressed ? SystemEventCode::MOUSE_BUTTON_PRESSED : SystemEventCode::MOUSE_BUTTON_RELEASED), nullptr, &context);
    }
}

void input_process_mouse_move(MousePos pos) {
    if (pos != state.mouse_curr.pos) {
        state.mouse_curr.pos = pos;
        EventContext context;
        context.data.i16[0] = pos.x;
        context.data.i16[1] = pos.y;
        event_execute_callback(static_cast<u8>(SystemEventCode::MOUSE_MOVED), nullptr, &context);
    }
}

void input_process_mouse_wheel(i8 z_delta) {
    EventContext context;
    context.data.i8[0] = z_delta;
    event_execute_callback(static_cast<u8>(SystemEventCode::MOUSE_WHEEL), nullptr, &context);
}

} // sf
