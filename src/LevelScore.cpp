#include "Internal.hpp"
#include <events/LevelScore.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyLevelScoreData(CCArray* scores, const matjson::Value& data) {
    for (auto score : CCArrayExt<GJUserScore*>(scores)) {
        auto id = score->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::LevelScoreEvent(score, id).post();
        }
    }
}

class $modify(UDALevelScoreManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDALevelScoreManager, GameLevelManager>>& self) {
        #ifdef GEODE_IS_WINDOWS
        (void)self.setHookPriority("GameLevelManager::handleIt", Priority::Replace);
        #else
        (void)self.setHookPriority("GameLevelManager::onGetLevelLeaderboardCompleted", Priority::Replace);
        #endif
    }

    void getLevelLeaderboard(GJGameLevel* level, LevelLeaderboardType type, LevelLeaderboardMode mode) {
        GameLevelManager::getLevelLeaderboard(level, type, mode);

        user_data::internal::fetchData(this, fmt::format("ll_{}_{}_{}", level->m_levelID.value(), (int)type, (int)mode), applyLevelScoreData);
    }

    #ifdef GEODE_IS_WINDOWS
    void handleIt(bool success, gd::string response, gd::string tag, GJHttpType type) {
        if (type != GJHttpType::GetLevelLeaderboard) return GameLevelManager::handleIt(success, response, tag, type);

        removeDLFromActive(tag.c_str());

        if (!success || response == "-1") {
            if (m_leaderboardManagerDelegate) {
                m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            }
            return;
        }

        auto scores = createAndGetScores(response, GJScoreType::LevelScore);
        storeSearchResult(scores, " ", tag.c_str());
        user_data::internal::applyData(scores, tag, applyLevelScoreData);
        if (m_leaderboardManagerDelegate) {
            m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
        }
    }
    #else
    void onGetLevelLeaderboardCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) {
                m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            }
            return;
        }

        auto scores = createAndGetScores(response, GJScoreType::LevelScore);
        storeSearchResult(scores, " ", tag.c_str());
        user_data::internal::applyData(scores, tag, applyLevelScoreData);
        if (m_leaderboardManagerDelegate) {
            m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
        }
    }
    #endif
};
