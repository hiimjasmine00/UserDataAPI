#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a friend request.
    class FriendRequestEvent : public geode::GlobalEvent<FriendRequestEvent, void(GJUserScore*), void(GJUserScore*), int> {
    public:
        using ObjectType = GJUserScore*;
        using GlobalEvent::GlobalEvent;
    };
}
