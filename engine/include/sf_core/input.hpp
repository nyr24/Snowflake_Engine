#pragma once

#include "sf_core/defines.hpp"

namespace sf {

enum struct MouseButton : u8 {
    LEFT,
    RIGHT,
    MIDDLE,
    COUNT
};

enum struct Key : u8 {
    BACKSPACE = 0x08,
    ENTER = 0x0D,
    TAB = 0x09,
    SHIFT = 0x10,
    CONTROL = 0x11,

    PAUSE = 0x13,
    CAPITAL = 0x14,

    ESCAPE = 0x1B,

    CONVERT = 0x1C,
    NONCONVERT = 0x1D,
    ACCEPT = 0x1E,
    MODECHANGE = 0x1F,

    SPACE = 0x20,
    PRIOR = 0x21,
    NEXT = 0x22,
    END = 0x23,
    HOME = 0x24,
    LEFT = 0x25,
    UP = 0x26,
    RIGHT = 0x27,
    DOWN = 0x28,
    SELECT = 0x29,
    PRINT = 0x2A,
    EXECUTE = 0x2B,
    SNAPSHOT = 0x2C,
    INSERT = 0x2D,
    DELETE = 0x2E,
    HELP = 0x2F,

    A = 0x41,
    B = 0x42,
    C = 0x43,
    D = 0x44,
    E = 0x45,
    F = 0x46,
    G = 0x47,
    H = 0x48,
    I = 0x49,
    J = 0x4A,
    K = 0x4B,
    L = 0x4C,
    M = 0x4D,
    N = 0x4E,
    O = 0x4F,
    P = 0x50,
    Q = 0x51,
    R = 0x52,
    S = 0x53,
    T = 0x54,
    U = 0x55,
    V = 0x56,
    W = 0x57,
    X = 0x58,
    Y = 0x59,
    Z = 0x5A,

    _0 = 0x0030,
    _1 = 0x0031,
    _2 = 0x0032,
    _3 = 0x0033,
    _4 = 0x0034,
    _5 = 0x0035,
    _6 = 0x0036,
    _7 = 0x0037,
    _8 = 0x0038,
    _9 = 0x0039,

    LWIN = 0x5B,
    RWIN = 0x5C,
    APPS = 0x5D,

    SLEEP = 0x5F,

    NUMPAD0 = 0x60,
    NUMPAD1 = 0x61,
    NUMPAD2 = 0x62,
    NUMPAD3 = 0x63,
    NUMPAD4 = 0x64,
    NUMPAD5 = 0x65,
    NUMPAD6 = 0x66,
    NUMPAD7 = 0x67,
    NUMPAD8 = 0x68,
    NUMPAD9 = 0x69,
    MULTIPLY = 0x6A,
    ADD = 0x6B,
    SEPARATOR = 0x6C,
    SUBTRACT = 0x6D,
    DECIMAL = 0x6E,
    DIVIDE = 0x6F,
    F1 = 0x70,
    F2 = 0x71,
    F3 = 0x72,
    F4 = 0x73,
    F5 = 0x74,
    F6 = 0x75,
    F7 = 0x76,
    F8 = 0x77,
    F9 = 0x78,
    F10 = 0x79,
    F11 = 0x7A,
    F12 = 0x7B,
    F13 = 0x7C,
    F14 = 0x7D,
    F15 = 0x7E,
    F16 = 0x7F,
    F17 = 0x80,
    F18 = 0x81,
    F19 = 0x82,
    F20 = 0x83,
    F21 = 0x84,
    F22 = 0x85,
    F23 = 0x86,
    F24 = 0x87,

    NUMLOCK = 0x90,
    SCROLL = 0x91,

    NUMPAD_EQUAL = 0x92,

    LSHIFT = 0xA0,
    RSHIFT = 0xA1,
    LCONTROL = 0xA2,
    RCONTROL = 0xA3,
    LMENU = 0xA4,
    RMENU = 0xA5,

    SEMICOLON = 0xBA,
    PLUS = 0xBB,
    COMMA = 0xBC,
    MINUS = 0xBD,
    PERIOD = 0xBE,
    SLASH = 0xBF,
    GRAVE = 0xC0,

    COUNT
};

struct MousePos {
    i16 x;
    i16 y;

    friend bool operator==(const MousePos& first, const MousePos& second) {
        return first.x == second.x && first.y == second.y;
    }
};

void input_update(f64 delta_time);

// keyboard input
SF_EXPORT bool input_is_key_down(Key key);
SF_EXPORT bool input_is_key_up(Key key);
SF_EXPORT bool input_was_key_down(Key key);
SF_EXPORT bool input_was_key_up(Key key);

void input_process_key(Key key, bool is_pressed);

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
