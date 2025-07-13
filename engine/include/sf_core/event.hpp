#pragma once

#include "sf_core/types.hpp"
#include "sf_ds/array_list.hpp"
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
using OnEventFn = bool(*)(u8 code, void* sender, void* listener_inst, EventContext context);

struct Event {
    void*       listener;
    OnEventFn   callback;
};

// System internal event codes. Application should use codes beyond 255.
enum SystemEventCode : u8 {
    EVENT_CODE_APPLICATION_QUIT,
    EVENT_CODE_KEY_PRESSED,
    EVENT_CODE_KEY_RELEASED,
    EVENT_CODE_BUTTON_PRESSED,
    EVENT_CODE_BUTTON_RELEASED,
    EVENT_CODE_MOUSE_MOVED,
    EVENT_CODE_MOUSE_WHEEL,
    EVENT_CODE_RESIZED,
    MAX_EVENT_CODE = 0xFF
};

struct EventSystemState {
    ArrayList<Event> event_lists[SystemEventCode::MAX_EVENT_CODE];

    EventSystemState();

    // NOTE: deside u8 or u16
    SF_EXPORT bool set_listener(u8 code, void* listener, OnEventFn on_event);
    SF_EXPORT bool unset_listener(u8 code, void* listener, OnEventFn on_event);
    SF_EXPORT bool fire_event(u8 code, void* sender, EventContext context);
};

} // sf
