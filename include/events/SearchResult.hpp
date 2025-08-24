#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a user search result.
    class SearchResultEvent : public geode::Event {
    protected:
        GJUserScore* m_score;
        int m_id;
    public:
        SearchResultEvent(GJUserScore* score, int id) : m_score(score), m_id(id) {}

        GJUserScore* getScore() const { return m_score; }
        int getID() const { return m_id; }
    };

    /// Represents a filter for SearchResultEvent.
    class SearchResultFilter : public geode::EventFilter<SearchResultEvent> {
    protected:
        int m_id;
    public:
        using Callback = void(GJUserScore*);

        /// Constructs a SearchResultFilter for a global listener.
        /// @param id The account ID to filter results by
        SearchResultFilter(int id = 0) : m_id(id) {}
        /// Constructs a SearchResultFilter for a node-based listener.
        /// @param id The account ID to filter results by
        SearchResultFilter(void*, int id = 0) : m_id(id) {}

        template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
        geode::ListenerResult handle(F&& fn, Event* event) {
            if (m_id <= 0 || event->getID() == m_id) fn(event->getScore());
            return geode::ListenerResult::Propagate;
        }
    };
}
