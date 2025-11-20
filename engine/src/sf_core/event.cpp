#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"

namespace sf {

static EventSystem* state_ptr{nullptr};

void EventSystem::create(EventSystem& out_system) {
    state_ptr = &out_system;
    out_system.event_lists.resize_to_capacity();
}

SF_EXPORT bool event_system_add_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state_ptr->event_lists[code].count(); ++i) {
        const Event& event = state_ptr->event_lists[code][i];
        if (event.callback == on_event_callback) {
            return false;
        }
    }

    state_ptr->event_lists[code].append_emplace(listener, on_event_callback);
    return true;
}

SF_EXPORT bool event_system_remove_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state_ptr->event_lists[code].count(); ++i) {
        const Event& event = state_ptr->event_lists[code][i];
        if (event.callback == on_event_callback) {
            state_ptr->event_lists[code].remove_unordered_at(i);
            return true;
        }
    }

    return false;
}

SF_EXPORT bool event_system_fire_event(u8 code, void* sender, Option<EventContext> context) {
    if (state_ptr->event_lists[code].count() == 0) {
        return false;
    }

    for (const Event& event : state_ptr->event_lists[code]) {
        if (event.callback(code, sender, event.listener, context)) {
            // event has been handled, do not send to other listeners
            return true;
        }
    }

    return false;
}

} // sf;
