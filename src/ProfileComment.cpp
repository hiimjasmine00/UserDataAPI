#include "Internal.hpp"
#include <events/ProfileComment.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDAProfileCommentManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAProfileCommentManager, GameLevelManager>>& self) {
        for (auto& [name, hook] : self.m_hooks) {
            if (name == "GameLevelManager::onGetAccountCommentsCompleted") hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
    }

    void getAccountComments(int id, int page, int total) {
        GameLevelManager::getAccountComments(id, page, total);

        fetchData<user_data::ProfileCommentFilter>(this, fmt::format("a_{}_{}", id, page));
    }

    void onGetAccountCommentsCompleted(gd::string response, gd::string tag) {
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

        auto comments = createAndGetAccountComments(split[0], levelIDFromCommentKey(tag.c_str()));
        auto pending = pendingKeys.contains(tag);
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            applyData<user_data::ProfileCommentEvent>(comment, comment->m_accountID, dataValues, pending);
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
    }
};
