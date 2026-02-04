#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a level or list comment.
    class CommentEvent : public geode::GlobalEvent<CommentEvent, void(GJComment*), void(GJComment*), int> {
    public:
        using ObjectType = GJComment*;
        using GlobalEvent::GlobalEvent;
    };
}
