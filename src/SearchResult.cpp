#include "Internal.hpp"
#include <events/SearchResult.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applySearchResultData(CCArray* scores, const matjson::Value& data) {
    for (auto score : CCArrayExt<GJUserScore*>(scores)) {
        auto id = score->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::SearchResultEvent(score, id).post();
        }
    }
}

class $modify(UDASearchResultManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDASearchResultManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetUsersCompleted", Priority::Replace);
    }

    void getUsers(GJSearchObject* object) {
        GameLevelManager::getUsers(object);

        user_data::internal::fetchData(this, object->getKey(), applySearchResultData);
    }

    void onGetUsersCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelManagerDelegate) {
                m_levelManagerDelegate->loadLevelsFailed(tag.c_str());
            }
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelManagerDelegate) {
                m_levelManagerDelegate->loadLevelsFailed(tag.c_str());
            }
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelManagerDelegate) {
            m_levelManagerDelegate->setupPageInfo(pageInfo, tag.c_str());
        }

        auto scores = createAndGetScores(split[0], GJScoreType::Search);
        storeSearchResult(scores, pageInfo, tag.c_str());
        user_data::internal::applyData(scores, tag, applySearchResultData);
        if (m_levelManagerDelegate) {
            m_levelManagerDelegate->loadLevelsFinished(scores, tag.c_str());
        }
    }
};
