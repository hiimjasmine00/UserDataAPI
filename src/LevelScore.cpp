#include "Internal.hpp"
#include <events/LevelScore.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDALevelScoreManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDALevelScoreManager, GameLevelManager>>& self) {
        for (auto& [name, hook] : self.m_hooks) {
            #ifdef GEODE_IS_WINDOWS
            if (name == "GameLevelManager::handleIt") hook->setPriority(Priority::Replace);
            #else
            if (name == "GameLevelManager::onGetLevelLeaderboardCompleted") hook->setPriority(Priority::Replace);
            #endif
            hook->setAutoEnable(enabled);
        }
    }

    void getLevelLeaderboard(GJGameLevel* level, LevelLeaderboardType type, LevelLeaderboardMode mode) {
        GameLevelManager::getLevelLeaderboard(level, type, mode);

        fetchData<user_data::LevelScoreFilter>(this, fmt::format("ll_{}_{}_{}", level->m_levelID.value(), (int)type, (int)mode));
    }

    #ifdef GEODE_IS_WINDOWS
    void handleIt(bool success, gd::string response, gd::string tag, GJHttpType type) {
        if (type == GJHttpType::GetLevelLeaderboard) processOnGetLevelLeaderboardCompleted(success ? response : "-1", tag);
        else GameLevelManager::handleIt(success, response, tag, type);
    }
    #else
    void onGetLevelLeaderboardCompleted(gd::string response, gd::string tag) {
        processOnGetLevelLeaderboardCompleted(response, tag);
    }
    #endif

    void processOnGetLevelLeaderboardCompleted(const gd::string& response, const gd::string& tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            return;
        }

        auto dataValues = prepareData(tag);

        auto scores = createAndGetScores(response, GJScoreType::LevelScore);
        auto pending = pendingKeys.contains(tag);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            applyData<user_data::LevelScoreEvent>(score, score->m_accountID, dataValues, pending);
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
    }
};
