#include "sf_core/event.hpp"
#include "sf_ds/array_list.hpp"

namespace sf {

EventSystemState::EventSystemState()
{
    for (ArrayList<Event>& list : event_lists) {
        list.reallocate(50);
    }
}

SF_EXPORT bool EventSystemState::set_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < event_lists[code].count(); ++i) {
        const Event& event = event_lists[code][i];
        if (event.listener == listener) {
            return false;
        }
    }

    event_lists[code].append_emplace(listener, on_event_callback);
    return true;
}

SF_EXPORT bool EventSystemState::unset_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < event_lists[code].count(); ++i) {
        const Event& event = event_lists[code][i];
        if (event.listener == listener && event.callback == on_event_callback) {
            event_lists[code].remove_unordered_at(i);
            return true;
        }
    }

    return false;
}

SF_EXPORT bool EventSystemState::fire_event(u8 code, void* sender, EventContext context) {
    if (event_lists[code].count() == 0) {
        return false;
    }

    for (u32 i{0}; i < event_lists[code].count(); ++i) {
        const Event& event = event_lists[code][i];
        if (event.callback(code, sender, event.listener, context)) {
            // event has been handled, do not send to other listeners
            return true;
        }
    }

    return false;
}

} // sf;
