#include "sf_containers/fixed_array.hpp"
#include "sf_core/defines.hpp"
#include "sf_core/event.hpp"
#include "sf_containers/dynamic_array.hpp"

namespace sf {

struct EventSystemState {
    static constexpr u32 DEFAULT_EVENT_COUNT{50};
    FixedArray<DynamicArray<Event>, static_cast<u32>(SystemEventCode::COUNT)> event_lists;

    EventSystemState()
    {
        event_lists.resize_to_capacity();
        
        for (DynamicArray<Event>& list : event_lists) {
            list.reserve(DEFAULT_EVENT_COUNT);
        }
    }
};

static EventSystemState state{};

SF_EXPORT bool event_set_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state.event_lists[code].count(); ++i) {
        const Event& event = state.event_lists[code][i];
        if (event.callback == on_event_callback) {
            return false;
        }
    }

    state.event_lists[code].append_emplace(listener, on_event_callback);
    return true;
}

SF_EXPORT bool event_unset_listener(u8 code, void* listener, OnEventFn on_event_callback) {
    for (u32 i{0}; i < state.event_lists[code].count(); ++i) {
        const Event& event = state.event_lists[code][i];
        if (event.callback == on_event_callback) {
            state.event_lists[code].remove_unordered_at(i);
            return true;
        }
    }

    return false;
}

SF_EXPORT bool event_execute_callbacks(u8 code, void* sender, Option<EventContext> context) {
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
