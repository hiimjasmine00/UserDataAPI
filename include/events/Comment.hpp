#pragma once
#include <Geode/GeneratedPredeclare.hpp>
#include <Geode/loader/Event.hpp>

namespace user_data {
    /// Represents an event for a level or list comment.
    class CommentEvent : public geode::Event {
    protected:
        GJComment* m_comment;
        int m_id;
    public:
        CommentEvent(GJComment* comment, int id) : m_comment(comment), m_id(id) {}

        GJComment* getComment() const { return m_comment; }
        int getID() const { return m_id; }
    };

    /// Represents a filter for CommentEvent.
    class CommentFilter : public geode::EventFilter<CommentEvent> {
    protected:
        int m_id;
    public:
        using Callback = void(GJComment*);

        /// Constructs a CommentFilter for a global listener.
        /// @param id The account ID to filter comments by
        CommentFilter(int id = 0) : m_id(id) {}
        /// Constructs a CommentFilter for a node-based listener.
        /// @param id The account ID to filter comments by
        CommentFilter(void*, int id = 0) : m_id(id) {}

        template <typename F> requires (std::is_invocable_r_v<void, F, GJComment*>)
        geode::ListenerResult handle(F&& fn, Event* event) {
            if (m_id <= 0 || event->getID() == m_id) fn(event->getComment());
            return geode::ListenerResult::Propagate;
        }
    };
}
