#include "Internal.hpp"
#include <events/GlobalScore.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDAGlobalScoreManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAGlobalScoreManager, GameLevelManager>>& self) {
        for (auto& [name, hook] : self.m_hooks) {
            if (name == "GameLevelManager::onGetLeaderboardScoresCompleted") hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
    }

    void getLeaderboardScores(const char* key) {
        GameLevelManager::getLeaderboardScores(key);

        fetchData<user_data::GlobalScoreFilter>(this, key);
    }

    void onGetLeaderboardScoresCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            return;
        }

        auto dataValues = prepareData(tag);

        auto scores = createAndGetScores(response, tag == "leaderboard_creator" ? GJScoreType::Creator : GJScoreType::Top);
        auto pending = pendingKeys.contains(tag);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            applyData<user_data::GlobalScoreEvent>(score, score->m_accountID, dataValues, pending);
        }

        if (tag == "leaderboard_friends") {
            auto scoresData = scores->data;
            auto scoresArr = reinterpret_cast<GJUserScore**>(scoresData->arr);
            std::sort(scoresArr, scoresArr + scoresData->num, [](GJUserScore* a, GJUserScore* b) {
                if (a->m_stars != b->m_stars) return a->m_stars > b->m_stars;
                if (a->m_demons != b->m_demons) return a->m_demons > b->m_demons;
                if (a->m_secretCoins != b->m_secretCoins) return a->m_secretCoins > b->m_secretCoins;
                return a->m_userCoins > b->m_userCoins;
            });
            auto rank = 1;
            for (auto score : CCArrayExt<GJUserScore*>(scores)) {
                score->m_playerRank = rank++;
            }
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
    }
};
