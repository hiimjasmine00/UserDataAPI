#include "Internal.hpp"
#include <events/Comment.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

void applyCommentData(CCArray* comments, const matjson::Value& data) {
    for (auto comment : CCArrayExt<GJComment*>(comments)) {
        if (auto score = comment->m_userScore) {
            auto id = score->m_accountID;
            if (auto value = data.get(fmt::to_string(id))) {
                for (auto& [k, v] : value.unwrap()) {
                    comment->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
                }
                user_data::CommentEvent(comment, id).post();
            }
        }
    }
}

class $modify(UDACommentManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDACommentManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetLevelCommentsCompleted", Priority::Replace);
    }

    void getLevelComments(int id, int page, int total, int mode, CommentKeyType type) {
        GameLevelManager::getLevelComments(id, page, total, mode, type);

        user_data::internal::fetchData(this, fmt::format("comment_{}_{}_{}_{}", id, page, mode, (int)type), applyCommentData);
    }

    void onGetLevelCommentsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelCommentDelegate) {
                m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            }
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelCommentDelegate) {
                m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            }
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelCommentDelegate) {
            m_levelCommentDelegate->setupPageInfo(pageInfo, tag.c_str());
        }

        auto comments = createAndGetLevelComments(split[0], levelIDFromCommentKey(tag.c_str()));
        storeCommentsResult(comments, pageInfo, tag.c_str());
        user_data::internal::applyData(comments, tag, applyCommentData);
        if (m_levelCommentDelegate) {
            m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
        }
    }
};
