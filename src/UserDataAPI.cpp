#include <argon/argon.hpp>
#include <Events.hpp>
#include <Geode/binding/FriendRequestDelegate.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/binding/UserInfoDelegate.hpp>
#include <Geode/binding/UserListDelegate.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <UserDataAPI.hpp>

using namespace geode::prelude;

bool enabled = true;
std::unordered_map<std::string, matjson::Value> userData;
std::unordered_set<std::string> pendingKeys;

$on_mod(Loaded) {
    auto res = argon::startAuth([](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    });
    if (res.isErr()) log::error("Failed to start Argon authentication: {}", res.unwrapErr());

    new EventListener([](const matjson::Value& data, const std::string& id) {
        user_data::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<matjson::Value, std::string>("v1/upload"_spr));
}

void user_data::upload(const matjson::Value& data, std::string_view id) {
    if (!enabled) return;

    auto res = argon::startAuth([data, id = std::string(id)](Result<std::string> res) {
        if (res.isOk()) {
            web::WebRequest()
                .bodyJSON(data)
                .header("Authorization", res.unwrap())
                .post(fmt::format("https://userdataapi.dankmeme.dev/v1/upload?id={}&mod={}",
                    GJAccountManager::get()->m_accountID, id))
                .listen([](web::WebResponse* res) {
                    if (!res->ok()) {
                        auto data = res->data();
                        log::error("Failed to upload user data: {}", std::string(data.begin(), data.end()));
                    }
                });
        }
        else if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    });
    if (res.isErr()) log::error("Failed to start Argon authentication: {}", res.unwrapErr());
}

std::unordered_map<int, matjson::Value> prepareData(const std::string& key) {
    std::unordered_map<int, matjson::Value> dataValues;
    if (auto it = userData.find(key); it != userData.end()) {
        for (auto& [k, v] : it->second) {
            auto id = 0;
            if (std::from_chars(k.data(), k.data() + k.size(), id).ec == std::errc()) dataValues[id] = v;
        }
        userData.erase(it);
    }
    return dataValues;
}

template <std::derived_from<Event> U, std::derived_from<CCNode> T>
void applyData(T* node, int id, const std::unordered_map<int, matjson::Value>& dataValues, bool pending) {
    if (auto it = dataValues.find(id); it != dataValues.end()) {
        for (auto& [k, v] : it->second) {
            node->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
        }
        U(node, it->first).post();
    }
    else if (pending) node->setUserObject("downloading"_spr, CCBool::create(true));
}

template <class U>
void fetchData(GameLevelManager* manager, const std::string& key, int id = 0) {
    using T = std::remove_pointer_t<function::Arg<0, typename U::Callback>>;
    pendingKeys.insert(key);
    web::WebRequest()
        .get(fmt::format("https://userdataapi.dankmeme.dev/v1/data{}", id > 0 ? fmt::format("?players={}", id) : ""))
        .listen([manager, id, key](web::WebResponse* res) {
            pendingKeys.erase(key);

            CCObject* object = nullptr;
            if (id > 0) {
                auto score = manager->userInfoForAccountID(id);
                if (score) score->setUserObject("downloading"_spr, nullptr);
                object = score;
            }
            else {
                auto objects = manager->getStoredOnlineLevels(key.c_str());
                for (auto node : CCArrayExt<CCNode*>(objects)) {
                    node->setUserObject("downloading"_spr, nullptr);
                }
                object = objects;
            }

            if (!res->ok()) {
                auto data = res->data();
                return log::error("Failed to get profile data: {}", std::string(data.begin(), data.end()));
            }

            if (auto json = res->json()) {
                auto data = json.unwrap();
                if (!data.isObject()) return log::error("Invalid profile data format");

                if (manager->isDLActive(key.c_str())) userData[key] = data;
                else {
                    std::unordered_map<int, matjson::Value> dataValues;
                    for (auto& [k, v] : data) {
                        auto id = 0;
                        if (std::from_chars(k.data(), k.data() + k.size(), id).ec == std::errc()) dataValues[id] = v;
                    }

                    if (id > 0) {
                        if (auto score = static_cast<T*>(object)) {
                            applyData<typename U::Event, T>(score, id, dataValues, false);
                        }
                    }
                    else {
                        for (auto node : CCArrayExt<CCNode*>(static_cast<CCArray*>(object))) {
                            auto accountID = 0;
                            if constexpr (std::is_same_v<T, GJComment>) {
                                auto comment = static_cast<GJComment*>(node);
                                accountID = comment->m_userScore ? comment->m_userScore->m_accountID : comment->m_accountID;
                            }
                            else if constexpr (std::is_same_v<T, GJUserScore>) {
                                accountID = static_cast<GJUserScore*>(node)->m_accountID;
                            }
                            if (accountID <= 0) continue;
                            applyData<typename U::Event, T>(static_cast<T*>(node), accountID, dataValues, false);
                        }
                    }
                }
            }
            else if (json.isErr()) {
                log::error("Failed to parse profile data: {}", json.unwrapErr());
            }
        });
}

#ifdef GEODE_IS_ANDROID
int scoreCompare(const void* a, const void* b);
#else
#ifdef GEODE_IS_WINDOWS
void* wrapFunction(uintptr_t address, const tulip::hook::WrapperMetadata& metadata) {
    auto wrapped = hook::createWrapper(reinterpret_cast<void*>(address), metadata);
    if (wrapped.isErr()) {
        throw std::runtime_error(wrapped.unwrapErr());
    }
    return wrapped.unwrap();
}
#else
void* wrapFunction(uintptr_t address, const tulip::hook::WrapperMetadata& metadata);
#endif

int scoreCompare(const void* a, const void* b) {
    using FunctionType = int(*)(const void*, const void*);
    static auto func = wrapFunction(
        base::get() + GEODE_WINDOWS(0x1408a0) GEODE_ARM_MAC(0x463f8c) GEODE_INTEL_MAC(0x504830) GEODE_IOS(0x8ba08),
        {
            .m_convention = hook::createConvention(tulip::hook::TulipConvention::Default),
            .m_abstract = tulip::hook::AbstractFunction::from(FunctionType(nullptr))
        }
    );
    return reinterpret_cast<FunctionType>(func)(a, b);
}
#endif

class $modify(UDAGameLevelManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAGameLevelManager, GameLevelManager>>& self) {
        enabled = argon::getBaseServerUrl().rfind("://www.boomlings.com/database") != std::string::npos;
        if (!enabled) log::error("GDPS detected, User Data API disabled");

        for (auto& [name, hook] : self.m_hooks) {
            if (!name.starts_with("GameLevelManager::get")) hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
    }

    void getLevelComments(int id, int page, int total, int mode, CommentKeyType type) {
        GameLevelManager::getLevelComments(id, page, total, mode, type);
        fetchData<user_data::CommentFilter>(this, fmt::format("comment_{}_{}_{}_{}", id, page, mode, (int)type),
            type == CommentKeyType::User ? accountIDForUserID(id) : 0);
    }

    void getUserList(UserListType type) {
        GameLevelManager::getUserList(type);
        fetchData<user_data::FriendFilter>(this, type == UserListType::Friends ? "get_friends" : "get_blocked");
    }

    void getFriendRequests(bool sent, int page, int total) {
        GameLevelManager::getFriendRequests(sent, page, total);
        fetchData<user_data::FriendRequestFilter>(this, fmt::format("fReq_{}_{}", (int)sent, page));
    }

    void getLeaderboardScores(const char* key) {
        GameLevelManager::getLeaderboardScores(key);
        fetchData<user_data::GlobalScoreFilter>(this, key);
    }

    void getLevelLeaderboard(GJGameLevel* level, LevelLeaderboardType type, LevelLeaderboardMode mode) {
        GameLevelManager::getLevelLeaderboard(level, type, mode);
        fetchData<user_data::LevelScoreFilter>(this, fmt::format("ll_{}_{}_{}", level->m_levelID, (int)type, (int)mode));
    }

    void getGJUserInfo(int accountID) {
        GameLevelManager::getGJUserInfo(accountID);
        fetchData<user_data::ProfileFilter>(this, fmt::format("account_{}", accountID), accountID);
    }

    void getAccountComments(int id, int page, int total) {
        GameLevelManager::getAccountComments(id, page, total);
        fetchData<user_data::ProfileCommentFilter>(this, fmt::format("a_{}_{}", id, page), id);
    }

    void getUsers(GJSearchObject* object) {
        GameLevelManager::getUsers(object);
        fetchData<user_data::SearchResultFilter>(this, object->getKey());
    }

    void onGetLevelCommentsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelCommentDelegate) m_levelCommentDelegate->setupPageInfo(pageInfo, tag.c_str());

        auto dataValues = prepareData(tag);

        auto comments = createAndGetLevelComments(split[0], levelIDFromCommentKey(tag.c_str()));
        auto pending = pendingKeys.contains(tag);
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            if (auto score = comment->m_userScore) {
                applyData<user_data::CommentEvent>(comment, score->m_accountID, dataValues, pending);
            }
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
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
                    request->m_isRead = !score->m_newFriendRequest;
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
            qsort(scoresData->arr, scoresData->num, sizeof(GJUserScore*), scoreCompare);
            auto rank = 1;
            for (auto score : CCArrayExt<GJUserScore*>(scores)) {
                score->m_playerRank = rank++;
            }
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());
    }

    #ifdef GEODE_IS_WINDOWS
    #define onGetLevelLeaderboardCompleted handleIt
    #endif

    void onGetLevelLeaderboardCompleted(GEODE_WINDOWS(bool success,) gd::string response, gd::string tag GEODE_WINDOWS(, GJHttpType type)) {
        #ifdef GEODE_IS_WINDOWS
        if (type != GJHttpType::GetLevelLeaderboard) return GameLevelManager::handleIt(success, response, tag, type);

        if (!success) response = "-1";
        #endif

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

    #ifdef GEODE_IS_WINDOWS
    #undef onGetLevelLeaderboardCompleted
    #endif

    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        auto accountID = getSplitIntFromKey(tag.c_str(), 1);
        if (response == "-1") {
            if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFailed(accountID);
            return;
        }

        auto dataValues = prepareData(tag);

        auto dict = responseToDict(response, false);
        auto score = GJUserScore::create(dict);
        if (!score) {
            if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFailed(accountID);
            return;
        }

        applyData<user_data::ProfileEvent>(score, score->m_accountID, dataValues, pendingKeys.contains(tag));

        storeUserInfo(score);
        if (score->m_friendReqStatus == 3) {
            auto request = GJFriendRequest::create(dict);
            request->m_accountID = score->m_accountID;
            storeFriendRequest(request);
        }

        if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFinished(score);
    }

    void onGetAccountCommentsCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto split = string::split(response, "#");
        if (split.size() < 2) {
            if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFailed(tag.c_str());
            return;
        }

        auto& pageInfo = split[1];
        if (m_levelCommentDelegate) m_levelCommentDelegate->setupPageInfo(pageInfo, tag.c_str());

        auto dataValues = prepareData(tag);

        auto comments = createAndGetAccountComments(split[0], levelIDFromCommentKey(tag.c_str()));
        auto pending = pendingKeys.contains(tag);
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            applyData<user_data::ProfileCommentEvent>(comment, comment->m_accountID, dataValues, pending);
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());
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
