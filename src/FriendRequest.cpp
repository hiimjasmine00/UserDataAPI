#include "Internal.hpp"
#include <events/FriendRequest.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/FriendRequestDelegate.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void applyFriendRequestData(CCArray* scores, const matjson::Value& data) {
    for (auto score : CCArrayExt<GJUserScore*>(scores)) {
        auto id = score->m_accountID;
        if (auto value = data.get(fmt::to_string(id))) {
            for (auto& [k, v] : value.unwrap()) {
                score->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            user_data::FriendRequestEvent(score, id).post();
        }
    }
}

class $modify(UDAFriendRequestManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAFriendRequestManager, GameLevelManager>>& self) {
        (void)self.setHookPriority("GameLevelManager::onGetFriendRequestsCompleted", Priority::Replace);
    }

    void getFriendRequests(bool sent, int page, int total) {
        GameLevelManager::getFriendRequests(sent, page, total);

        user_data::internal::fetchData(this, fmt::format("fReq_{}_{}", (int)sent, page), applyFriendRequestData);
    }

    void onGetFriendRequestsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_friendRequestDelegate) {
                m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), GJErrorCode::NotFound);
            }
            return;
        }

        if (atoi(response.c_str()) < 0) {
            if (m_friendRequestDelegate) {
                m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), (GJErrorCode)-2);
            }
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_friendRequestDelegate) {
                m_friendRequestDelegate->loadFRequestsFailed(tag.c_str(), GJErrorCode::NotFound);
            }
            return;
        }

        auto& pageInfo = split[1];
        if (m_friendRequestDelegate) {
            m_friendRequestDelegate->setupPageInfo(pageInfo, tag.c_str());
        }

        auto scores = CCArray::create();
        auto sent = getSplitIntFromKey(tag.c_str(), 1);
        for (auto& result : string::split(split[0], "|")) {
            auto dict = responseToDict(result, false);
            if (auto score = GJUserScore::create(dict)) {
                if (auto request = GJFriendRequest::create(dict)) {
                    score->m_friendReqStatus = sent ? 4 : 3;
                    request->m_is36 = !score->m_newFriendRequest;
                    request->m_accountID = score->m_accountID;
                    scores->addObject(score);
                    storeFriendRequest(request);
                    storeUserName(score->m_userID, score->m_accountID, score->m_userName);
                }
            }
        }

        storeCommentsResult(scores, pageInfo, tag.c_str());
        user_data::internal::applyData(scores, tag, applyFriendRequestData);
        if (m_friendRequestDelegate) {
            m_friendRequestDelegate->loadFRequestsFinished(scores, tag.c_str());
        }
    }
};
