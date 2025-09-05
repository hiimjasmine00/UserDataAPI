#include "Internal.hpp"
#include <events/GlobalScore.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyGlobalScoreData(CCArray* scores, const matjson::Value& data) {
    for (auto score : CCArrayExt<GJUserScore*>(scores)) {
        auto id = score->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::GlobalScoreEvent(score, id).post();
        }
    }
}

class $modify(UDAGlobalScoreManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAGlobalScoreManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetLeaderboardScoresCompleted", Priority::Replace);
    }

    void getLeaderboardScores(const char* key) {
        GameLevelManager::getLeaderboardScores(key);

        user_data::internal::fetchData(this, key, applyGlobalScoreData);
    }

    void onGetLeaderboardScoresCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) {
                m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            }
            return;
        }

        auto scores = createAndGetScores(response, tag == "leaderboard_creator" ? GJScoreType::Creator : GJScoreType::Top);
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
        user_data::internal::applyData(scores, tag, applyGlobalScoreData);
        if (m_leaderboardManagerDelegate) {
            m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
        }
    }
};
