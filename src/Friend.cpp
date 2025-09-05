#include "Internal.hpp"
#include <events/Friend.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/UserListDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyFriendData(CCArray* scores, const matjson::Value& data) {
    for (auto score : CCArrayExt<GJUserScore*>(scores)) {
        auto id = score->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::FriendEvent(score, id).post();
        }
    }
}

class $modify(UDAFriendManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAFriendManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetUserListCompleted", Priority::Replace);
    }

    void getUserList(UserListType type) {
        GameLevelManager::getUserList(type);

        user_data::internal::fetchData(this, type == UserListType::Friends ? "get_friends" : "get_blocked", applyFriendData);
    }

    void onGetUserListCompleted(gd::string response, gd::string tag) {
        auto type = tag == "get_friends" ? UserListType::Friends : UserListType::Blocked;
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_userListDelegate) {
                m_userListDelegate->getUserListFailed(type, GJErrorCode::NotFound);
            }
            return;
        }

        if (atoi(response.c_str()) < 0) {
            if (m_userListDelegate) {
                m_userListDelegate->getUserListFailed(type, (GJErrorCode)-2);
            }
            return;
        }

        auto scores = CCArray::create();
        auto status = type == UserListType::Blocked ? 2 : 1;
        for (auto& result : string::split(response, "|")) {
            auto score = GJUserScore::create(responseToDict(result, false));
            score->m_friendReqStatus = status;
            if (score->m_accountID > 0 && score->m_userID > 0) {
                scores->addObject(score);
                storeUserName(score->m_userID, score->m_accountID, score->m_userName);
            }
        }

        if (scores->count() == 0) {
            if (m_userListDelegate) {
                m_userListDelegate->getUserListFailed(type, GJErrorCode::NotFound);
            }
            return;
        }

        m_storedLevels->setObject(scores, tag);
        user_data::internal::applyData(scores, tag, applyFriendData);
        if (m_userListDelegate) {
            m_userListDelegate->getUserListFinished(scores, type);
        }
    }
};
