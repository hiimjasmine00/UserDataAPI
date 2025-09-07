#include "Internal.hpp"
#include <events/SearchResult.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDASearchResultManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDASearchResultManager, GameLevelManager>>& self) {
        for (auto& [name, hook] : self.m_hooks) {
            if (name == "GameLevelManager::onGetUsersCompleted") hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
    }

    void getUsers(GJSearchObject* object) {
        GameLevelManager::getUsers(object);

        fetchData<user_data::SearchResultFilter>(this, object->getKey());
    }

    void onGetUsersCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelManagerDelegate) m_levelManagerDelegate->loadLevelsFailed(tag.c_str());
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelManagerDelegate) m_levelManagerDelegate->loadLevelsFailed(tag.c_str());
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelManagerDelegate) m_levelManagerDelegate->setupPageInfo(pageInfo, tag.c_str());

        auto dataValues = prepareData(tag);

        auto scores = createAndGetScores(split[0], GJScoreType::Search);
        auto pending = pendingKeys.contains(tag);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            applyData<user_data::SearchResultEvent>(score, score->m_accountID, dataValues, pending);
        }

        storeSearchResult(scores, pageInfo, tag.c_str());
        if (m_levelManagerDelegate) m_levelManagerDelegate->loadLevelsFinished(scores, tag.c_str());
    }
};
