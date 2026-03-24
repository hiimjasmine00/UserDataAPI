#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a level leaderboard score.
    class LevelScoreEvent : public geode::GlobalEvent<LevelScoreEvent, bool(GJUserScore*), bool(GJUserScore*), int> {
    public:
        using ObjectType = GJUserScore*;
        using GlobalEvent::GlobalEvent;
    };
}
