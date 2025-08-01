#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"
#include "sf_ds/array_list.hpp"

namespace sf {

struct EventSystemState {
    ArrayList<Event> event_lists[static_cast<u8>(SystemEventCode::COUNT)];
    EventSystemState()
    {
        for (ArrayList<Event>& list : event_lists) {
            list.reallocate(50);
        }
    }
};

static EventSystemState state{};

SF_EXPORT bool event_set_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state.event_lists[code].count(); ++i) {
        const Event& event = state.event_lists[code][i];
        if (event.listener == listener) {
            return false;
        }
    }

    state.event_lists[code].append_emplace(listener, on_event_callback);
    return true;
}

SF_EXPORT bool event_unset_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state.event_lists[code].count(); ++i) {
        const Event& event = state.event_lists[code][i];
        if (event.listener == listener && event.callback == on_event_callback) {
            state.event_lists[code].remove_unordered_at(i);
            return true;
        }
    }

    return false;
}

SF_EXPORT bool event_execute_callback(u8 code, void* sender, EventContext* context) {
    if (state.event_lists[code].count() == 0) {
        return false;
    }

    for (u32 i{0}; i < state.event_lists[code].count(); ++i) {
        const Event& event = state.event_lists[code][i];
        if (event.callback(code, sender, event.listener, context)) {
            // event has been handled, do not send to other listeners
            return true;
        }
    }

    return false;
}

} // sf;
