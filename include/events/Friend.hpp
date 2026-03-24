#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a friend or blocked user.
    class FriendEvent : public geode::GlobalEvent<FriendEvent, bool(GJUserScore*), bool(GJUserScore*), int> {
    public:
        using ObjectType = GJUserScore*;
        using GlobalEvent::GlobalEvent;
    };
}
