#pragma once

#include "sf_allocators/linear_allocator.hpp"
#include "sf_containers/dynamic_array.hpp"
#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_containers/optional.hpp"

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

// Should return true if handled.
using OnEventFn = bool(*)(u8 code, void* sender, void* listener_inst, Option<EventContext> context);

struct Event {
    void*       listener;
    OnEventFn   callback;
};

struct EventSystemState {
    static constexpr u32 MAX_EVENT_COUNT{50};
    FixedArray<DynamicArray<Event, LinearAllocator>, static_cast<u32>(SystemEventCode::COUNT)> event_lists;

    static consteval u32 get_memory_requirement() {
        return sizeof(Event) * MAX_EVENT_COUNT * SystemEventCode::COUNT;
    }
    
    static void create(LinearAllocator& system_allocator, EventSystemState& out_system);
    ~EventSystemState() {
        for (auto& list : event_lists) {
            list.clear();
        }
    }
};

void event_system_init_internal_state(EventSystemState* state);
SF_EXPORT bool event_system_add_listener(u8 code, void* listener, OnEventFn on_event);
SF_EXPORT bool event_system_remove_listener(u8 code, void* listener, OnEventFn on_event);
SF_EXPORT bool event_system_fire_event(u8 code, void* sender, Option<EventContext> context);

} // sf
