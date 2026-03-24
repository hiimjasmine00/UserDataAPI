#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a global leaderboard score.
    class GlobalScoreEvent : public geode::GlobalEvent<GlobalScoreEvent, bool(GJUserScore*), bool(GJUserScore*), int> {
    public:
        using ObjectType = GJUserScore*;
        using GlobalEvent::GlobalEvent;
    };
}
