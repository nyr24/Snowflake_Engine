#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"
#include "sf_containers/dynamic_array.hpp"

namespace sf {

static EventSystemState* state_ptr{nullptr};

void EventSystemState::create(LinearAllocator& system_allocator, EventSystemState& out_system) {
    out_system.event_lists.resize_to_capacity();

    for (auto& list : out_system.event_lists) {
        list.set_allocator(&system_allocator);
        list.reserve(MAX_EVENT_COUNT);
    }
}

void event_system_init_internal_state(EventSystemState* event_state) {
    state_ptr = event_state;
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
