#include "Internal.hpp"
#include <events/FriendRequest.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/FriendRequestDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>

using namespace geode::prelude;

class $modify(UDAFriendRequestManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAFriendRequestManager, GameLevelManager>>& self) {
        for (auto& [name, hook] : self.m_hooks) {
            if (name == "GameLevelManager::onGetFriendRequestsCompleted") hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
    }

    void getFriendRequests(bool sent, int page, int total) {
        GameLevelManager::getFriendRequests(sent, page, total);

        fetchData<user_data::FriendRequestFilter>(this, fmt::format("fReq_{}_{}", (int)sent, page));
    }

    void onGetFriendRequestsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), GJErrorCode::GenericError);
            return;
        }

        if (atoi(response.c_str()) < 0) {
            if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), GJErrorCode::NotFound);
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), GJErrorCode::GenericError);
            return;
        }

        auto& pageInfo = split[1];
        if (m_friendRequestDelegate) m_friendRequestDelegate->setupPageInfo(pageInfo, tag.c_str());

        auto dataValues = prepareData(tag);

        auto scores = CCArray::create();
        auto pending = pendingKeys.contains(tag);
        auto status = getSplitIntFromKey(tag.c_str(), 1) != 0 ? 4 : 3;
        for (auto& result : string::split(split[0], "|")) {
            auto dict = responseToDict(result, false);
            if (auto score = GJUserScore::create(dict)) {
                if (auto request = GJFriendRequest::create(dict)) {
                    score->m_friendReqStatus = status;
                    request->m_is36 = !score->m_newFriendRequest;
                    request->m_accountID = score->m_accountID;
                    applyData<user_data::FriendRequestEvent>(score, score->m_accountID, dataValues, pending);
                    scores->addObject(score);
                    storeFriendRequest(request);
                    storeUserName(score->m_userID, score->m_accountID, score->m_userName);
                }
            }
        }

        storeCommentsResult(scores, pageInfo, tag.c_str());
        if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFinished(scores, tag.c_str());
    }
};
