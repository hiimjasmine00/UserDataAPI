#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a profile comment.
    class ProfileCommentEvent : public geode::GlobalEvent<ProfileCommentEvent, bool(GJComment*), bool(GJComment*), int> {
    public:
        using ObjectType = GJComment*;
        using GlobalEvent::GlobalEvent;
    };
}
