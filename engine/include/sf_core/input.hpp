#pragma once

#include "sf_core/defines.hpp"

namespace sf {

enum struct MouseButton : u8 {
    LEFT,
    RIGHT,
    MIDDLE,
    COUNT
};

struct MousePos {
    f32 x;
    f32 y;

    friend bool operator==(const MousePos& first, const MousePos& second) {
        return first.x == second.x && first.y == second.y;
    }
};

struct MouseDelta {
    static constexpr f32 SENSITIVITY{0.1f};
    f32 x;
    f32 y;    
};

void input_update(f64 delta_time);

// keyboard input
SF_EXPORT bool input_is_key_down(i32 key);
SF_EXPORT bool input_is_key_up(i32 key);
SF_EXPORT bool input_was_key_down(i32 key);
SF_EXPORT bool input_was_key_up(i32 key);

void input_process_key(i32 key, bool is_pressed);

// mouse input
SF_EXPORT bool input_is_button_down(MouseButton button);
SF_EXPORT bool input_is_button_up(MouseButton button);
SF_EXPORT bool input_was_button_down(MouseButton button);
SF_EXPORT bool input_was_button_up(MouseButton button);
SF_EXPORT MousePos input_get_mouse_position();
SF_EXPORT MousePos input_get_previous_mouse_position();

void input_process_mouse_button(MouseButton button, bool pressed);
void input_process_mouse_move(MousePos pos);
void input_process_mouse_wheel(i8 z_delta);

} // sf
