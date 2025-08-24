#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a user profile.
    class ProfileEvent : public geode::Event {
    protected:
        GJUserScore* m_score;
        int m_id;
    public:
        ProfileEvent(GJUserScore* score, int id) : m_score(score), m_id(id) {}

        GJUserScore* getScore() const { return m_score; }
        int getID() const { return m_id; }
    };

    /// Represents a filter for ProfileEvent.
    class ProfileFilter : public geode::EventFilter<ProfileEvent> {
    protected:
        int m_id;
    public:
        using Callback = void(GJUserScore*);

        /// Constructs a ProfileFilter for a global listener.
        /// @param id The account ID to filter profiles by
        ProfileFilter(int id = 0) : m_id(id) {}
        /// Constructs a ProfileFilter for a node-based listener.
        /// @param id The account ID to filter profiles by
        ProfileFilter(void*, int id = 0) : m_id(id) {}

        template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
        geode::ListenerResult handle(F&& fn, Event* event) {
            if (m_id <= 0 || event->getID() == m_id) fn(event->getScore());
            return geode::ListenerResult::Propagate;
        }
    };
}
