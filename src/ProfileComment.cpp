#include "Internal.hpp"
#include <events/ProfileComment.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyProfileCommentData(CCArray* comments, const matjson::Value& data) {
    for (auto comment : CCArrayExt<GJComment*>(comments)) {
        auto id = comment->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                comment->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::ProfileCommentEvent(comment, id).post();
        }
    }
}

class $modify(UDAProfileCommentManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAProfileCommentManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetAccountCommentsCompleted", Priority::Replace);
    }

    void getAccountComments(int id, int page, int total) {
        GameLevelManager::getAccountComments(id, page, total);

        user_data::internal::fetchData(this, fmt::format("a_{}_{}", id, page), applyProfileCommentData);
    }

    void onGetAccountCommentsCompleted(gd::string response, gd::string tag) {
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

        auto comments = createAndGetAccountComments(split[0], levelIDFromCommentKey(tag.c_str()));
        storeCommentsResult(comments, pageInfo, tag.c_str());
        user_data::internal::applyData(comments, tag, applyProfileCommentData);
        if (m_levelCommentDelegate) {
            m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
        }
    }
};
