#include "Internal.hpp"
#include <events/Comment.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDACommentManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDACommentManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetLevelCommentsCompleted", Priority::Replace);
    }

    void getLevelComments(int id, int page, int total, int mode, CommentKeyType type) {
        GameLevelManager::getLevelComments(id, page, total, mode, type);

        fetchData<user_data::CommentFilter>(this, fmt::format("comment_{}_{}_{}_{}", id, page, mode, (int)type));
    }

    void onGetLevelCommentsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelCommentDelegate) m_levelCommentDelegate->setupPageInfo(pageInfo, tag.c_str());

        auto dataValues = prepareData(tag);

        auto comments = createAndGetLevelComments(split[0], levelIDFromCommentKey(tag.c_str()));
        auto pending = pendingKeys.contains(tag);
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            if (auto score = comment->m_userScore) {
                applyData<user_data::CommentEvent>(comment, score->m_accountID, dataValues, pending);
            }
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
    }
};
