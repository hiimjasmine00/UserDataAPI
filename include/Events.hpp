#pragma once
#include "events/Comment.hpp"
#include "events/Friend.hpp"
#include "events/FriendRequest.hpp"
#include "events/GlobalScore.hpp"
#include "events/LevelScore.hpp"
#include "events/Profile.hpp"
#include "events/ProfileComment.hpp"
#include "events/SearchResult.hpp"
#include <Geode/binding/CommentCell.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/GJLevelScoreCell.hpp>
#include <Geode/binding/GJRequestCell.hpp>
#include <Geode/binding/GJScoreCell.hpp>
#include <Geode/binding/GJUserCell.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/ProfilePage.hpp>

namespace user_data {
    /// Runs a function on a CommentCell once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param cell The CommentCell to handle
    /// @param fn The function to run on the CommentCell's comment
    template <typename F> requires (std::is_invocable_r_v<void, F, GJComment*>)
    inline void handleCommentCell(CommentCell* cell, F&& fn) {
        if (!cell) return;

        auto comment = cell->m_comment;
        if (!comment) return;

        auto accountComment = cell->m_accountComment;
        auto score = comment->m_userScore;
        if (!accountComment && !score) return;

        if (comment->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            if (accountComment) cell->addEventListener<ProfileCommentFilter>(fn, comment->m_accountID);
            else if (score) cell->addEventListener<CommentFilter>(fn, score->m_accountID);
        }
        else fn(comment);
    }

    /// Runs a function on a GJLevelScoreCell once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param cell The GJLevelScoreCell to handle
    /// @param fn The function to run on the GJLevelScoreCell's score
    template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
    inline void handleLevelScoreCell(GJLevelScoreCell* cell, F&& fn) {
        if (!cell) return;

        auto score = cell->m_userScore;
        if (!score) return;

        if (score->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            cell->addEventListener<LevelScoreFilter>(fn, score->m_accountID);
        }
        else fn(score);
    }

    /// Runs a function on a ProfilePage once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param page The ProfilePage to handle
    /// @param fn The function to run on the ProfilePage's score
    template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
    inline void handleProfilePage(ProfilePage* page, F&& fn) {
        if (!page) return;

        auto score = page->m_score;
        if (!score) return;

        if (score->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            score->addEventListener<ProfileFilter>(fn, score->m_accountID);
        }
        else fn(score);
    }

    /// Runs a function on a GJRequestCell once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param cell The GJRequestCell to handle
    /// @param fn The function to run on the GJRequestCell's score
    template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
    inline void handleRequestCell(GJRequestCell* cell, F&& fn) {
        if (!cell) return;

        auto score = cell->m_score;
        if (!score) return;

        if (score->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            cell->addEventListener<FriendRequestFilter>(fn, score->m_accountID);
        }
        else fn(score);
    }

    /// Runs a function on a GJScoreCell once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param cell The GJScoreCell to handle
    /// @param fn The function to run on the GJScoreCell's score
    template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
    inline void handleScoreCell(GJScoreCell* cell, F&& fn) {
        if (!cell) return;

        auto score = cell->m_score;
        if (!score) return;

        if (score->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            if (score->m_scoreType == 2) cell->addEventListener<SearchResultFilter>(fn, score->m_accountID);
            else cell->addEventListener<GlobalScoreFilter>(fn, score->m_accountID);
        }
        else fn(score);
    }

    /// Runs a function on a GJUserCell once its user data has been downloaded.
    /// If the data has already been downloaded, the function is run immediately.
    /// @param cell The GJUserCell to handle
    /// @param fn The function to run on the GJUserCell's score
    template <typename F> requires (std::is_invocable_r_v<void, F, GJUserScore*>)
    inline void handleUserCell(GJUserCell* cell, F&& fn) {
        if (!cell) return;

        auto score = cell->m_userScore;
        if (!score) return;

        if (score->getUserObject("hiimjasmine00.user_data_api/downloading")) {
            switch (score->m_friendReqStatus) {
                case 1: case 2:
                    cell->addEventListener<FriendFilter>(fn, score->m_accountID);
                    break;
                case 4:
                    cell->addEventListener<FriendRequestFilter>(fn, score->m_accountID);
                    break;
            }
        }
        else fn(score);
    }
}
