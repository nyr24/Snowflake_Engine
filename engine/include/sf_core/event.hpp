#pragma once

#include "sf_core/types.hpp"
#include "sf_platform/platform.hpp"

namespace sf {

struct EventContext {
    union Data {
        u64 u64[2];
        i64 i64[2];
        f64 f64[2];

        u32 u32[4];
        i32 i32[4];
        f32 f32[4];

        u16 u16[8];
        i16 i16[8];

        u8 u8[16];
        i8 i8[16];
    };

    Data data;
};

// Should return true if handled.
using OnEventFn = bool(*)(u8 code, void* sender, void* listener_inst, EventContext* context);

struct Event {
    void*       listener;
    OnEventFn   callback;
};

// System internal event codes. Application should use codes beyond 255.
enum SystemEventCode : u8 {
    APPLICATION_QUIT,
    KEY_PRESSED,
    KEY_RELEASED,
    MOUSE_BUTTON_PRESSED,
    MOUSE_BUTTON_RELEASED,
    MOUSE_MOVED,
    MOUSE_WHEEL,
    RESIZED,
    COUNT = 0xFF
};

SF_EXPORT bool event_set_listener(u8 code, void* listener, OnEventFn on_event);
SF_EXPORT bool event_unset_listener(u8 code, void* listener, OnEventFn on_event);
SF_EXPORT bool event_execute_callback(u8 code, void* sender, EventContext* context);

} // sf
