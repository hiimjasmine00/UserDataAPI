#include "Internal.hpp"
#include <events/Profile.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/UserInfoDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyProfileData(CCNode* node, const matjson::Value& data) {
    auto score = static_cast<GJUserScore*>(node);
    auto id = score->m_accountID;
    if (auto value = data.get(fmt::to_string(id))) {
        for (auto& [k, v] : value.unwrap()) {
            score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
        }
        user_data::ProfileEvent(score, id).post();
    }
}

class $modify(UDAProfileManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAProfileManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetGJUserInfoCompleted", Priority::Replace);
    }

    void getGJUserInfo(int accountID) {
        GameLevelManager::getGJUserInfo(accountID);

        user_data::internal::fetchData(this, accountID, fmt::format("account_{}", accountID), applyProfileData);
    }

    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        auto accountID = getSplitIntFromKey(tag.c_str(), 1);
        if (response == "-1") {
            if (m_userInfoDelegate) {
                m_userInfoDelegate->getUserInfoFailed(accountID);
            }
            return;
        }

        auto dict = responseToDict(response, false);
        auto score = GJUserScore::create(dict);
        if (!score) {
            if (m_userInfoDelegate) {
                m_userInfoDelegate->getUserInfoFailed(accountID);
            }
            return;
        }

        storeUserInfo(score);
        if (score->m_friendReqStatus == 3) {
            auto request = GJFriendRequest::create(dict);
            request->m_accountID = score->m_accountID;
            storeFriendRequest(request);
        }

        user_data::internal::applyData(score, tag, applyProfileData);
        if (m_userInfoDelegate) {
            m_userInfoDelegate->getUserInfoFinished(score);
        }
    }
};
