#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a profile comment.
    class ProfileCommentEvent : public geode::Event {
    protected:
        GJComment* m_comment;
        int m_id;
    public:
        ProfileCommentEvent(GJComment* comment, int id) : m_comment(comment), m_id(id) {}

        GJComment* getComment() const { return m_comment; }
        int getID() const { return m_id; }
    };

    /// Represents a filter for ProfileCommentEvent.
    class ProfileCommentFilter : public geode::EventFilter<ProfileCommentEvent> {
    protected:
        int m_id;
    public:
        using Callback = void(GJComment*);

        /// Constructs a ProfileCommentFilter for a global listener.
        /// @param id The account ID to filter comments by
        ProfileCommentFilter(int id = 0) : m_id(id) {}
        /// Constructs a ProfileCommentFilter for a node-based listener.
        /// @param id The account ID to filter comments by
        ProfileCommentFilter(void*, int id = 0) : m_id(id) {}

        template <typename F> requires (std::is_invocable_r_v<void, F, GJComment*>)
        geode::ListenerResult handle(F&& fn, Event* event) {
            if (m_id <= 0 || event->getID() == m_id) fn(event->getComment());
            return geode::ListenerResult::Propagate;
        }
    };
}
