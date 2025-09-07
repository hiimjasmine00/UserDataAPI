#include "Internal.hpp"
#include <events/Friend.hpp>
#include <Geode/binding/UserListDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDAFriendManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAFriendManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetUserListCompleted", Priority::Replace);
    }

    void getUserList(UserListType type) {
        GameLevelManager::getUserList(type);

        fetchData<user_data::FriendFilter>(this, type == UserListType::Friends ? "get_friends" : "get_blocked");
    }

    void onGetUserListCompleted(gd::string response, gd::string tag) {
        auto type = tag == "get_friends" ? UserListType::Friends : UserListType::Blocked;
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_userListDelegate) m_userListDelegate->getUserListFailed(type, GJErrorCode::GenericError);
            return;
        }

        if (atoi(response.c_str()) < 0) {
            if (m_userListDelegate) m_userListDelegate->getUserListFailed(type, GJErrorCode::NotFound);
            return;
        }

        auto dataValues = prepareData(tag);

        auto scores = CCArray::create();
        auto pending = pendingKeys.contains(tag);
        auto status = type == UserListType::Blocked ? 2 : 1;
        for (auto& result : string::split(response, "|")) {
            auto score = GJUserScore::create(responseToDict(result, false));
            score->m_friendReqStatus = status;
            if (score->m_accountID > 0 && score->m_userID > 0) {
                applyData<user_data::FriendEvent>(score, score->m_accountID, dataValues, pending);
                scores->addObject(score);
                storeUserName(score->m_userID, score->m_accountID, score->m_userName);
            }
        }

        if (scores->count() == 0) {
            if (m_userListDelegate) m_userListDelegate->getUserListFailed(type, GJErrorCode::GenericError);
            return;
        }

        m_storedLevels->setObject(scores, tag);
        if (m_userListDelegate) m_userListDelegate->getUserListFinished(scores, type);
    }
};
