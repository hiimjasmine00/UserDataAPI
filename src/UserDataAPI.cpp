#include <argon/argon.hpp>
#include <Events.hpp>
#include <Geode/binding/FriendRequestDelegate.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/binding/UserInfoDelegate.hpp>
#include <Geode/binding/UserListDelegate.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <jasmine/convert.hpp>
#include <jasmine/gdps.hpp>
#include <jasmine/web.hpp>
#define GEODE_DEFINE_EVENT_EXPORTS
#include <UserDataAPI.hpp>

using namespace geode::prelude;

bool enabled = true;
std::vector<ZStringView> mods;

$on_mod(Loaded) {
    spawn(argon::startAuth(), [](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    });
}

$on_game(ModsLoaded) {
    for (auto mod : Loader::get()->getAllMods()) {
        if (mod->isLoaded() && std::ranges::contains(mod->getMetadata().getDependencies(), GEODE_MOD_ID, &ModMetadata::Dependency::getID)) {
            auto modID = mod->getID();
            if (modID == "camila314.comment-colors") modID = "camila314.comment-color";
            mods.push_back(modID);
        }
    }
}

void user_data::upload(matjson::Value data, std::string id) {
    if (!enabled) return;

    spawn(argon::startAuth(), [data = std::move(data), id = std::move(id)](Result<std::string> res) {
        if (res.isOk()) {
            spawn(
                web::WebRequest()
                    .bodyJSON(data)
                    .header("Authorization", std::move(res).unwrap())
                    .post(fmt::format("https://userdataapi.dankmeme.dev/v1/upload?id={}&mod={}", GJAccountManager::get()->m_accountID, id)),
                [](web::WebResponse res) {
                    if (!res.ok()) log::error("Failed to upload user data: {}", jasmine::web::getString(res));
                }
            );
        }
        else if (res.isErr()) {
            log::error("Argon authentication failed: {}", res.unwrapErr());
        }
    });
}

template <class U, class T>
void applyData(T* node, int id, const std::unordered_map<int, matjson::Value>& dataValues) {
    node->setUserFlag("downloading"_spr, false);
    if (auto it = dataValues.find(id); it != dataValues.end()) {
        for (auto& [k, v] : it->second) {
            node->setUserObject(fmt::format("{}"_spr, k), ObjWrapper<matjson::Value>::create(v));
        }
        U(id).send(node);
    }
}

template <class U>
void fetchData(CCObject* object) {
    using T = U::ObjectType;

    if (!object) return;
    if constexpr (!std::is_same_v<U, user_data::ProfileEvent>) {
        if (static_cast<CCArray*>(object)->count() == 0) return;
    }

    std::string url;
    if constexpr (std::is_same_v<U, user_data::ProfileEvent>) {
        url = fmt::format("https://userdataapi.dankmeme.dev/v1/data?players={}&mods={}",
            static_cast<GJUserScore*>(object)->m_accountID, fmt::join(mods, ","));
    }
    else {
        std::set<int> ids;
        for (auto obj : CCArrayExt<CCNode*>(static_cast<CCArray*>(object))) {
            if constexpr (std::is_same_v<U, user_data::CommentEvent>) {
                if (auto score = static_cast<GJComment*>(obj)->m_userScore) ids.insert(score->m_accountID);
            }
            else if constexpr (std::is_same_v<U, user_data::ProfileCommentEvent>) {
                ids.insert(static_cast<GJComment*>(obj)->m_accountID);
            }
            else {
                ids.insert(static_cast<GJUserScore*>(obj)->m_accountID);
            }
        }
        url = fmt::format("https://userdataapi.dankmeme.dev/v1/data?players={}&mods={}", fmt::join(ids, ","), fmt::join(mods, ","));
    }

    spawn(web::WebRequest().get(std::move(url)), [objectRef = WeakRef(object)](web::WebResponse res) {
        auto object = objectRef.lock().data();
        if (!object) return;

        if (!res.ok()) return log::error("Failed to get profile data: {}", jasmine::web::getString(res));

        auto json = res.json();
        if (json.isErr()) return log::error("Failed to parse profile data: {}", json.unwrapErr());

        auto data = std::move(json).unwrap();
        if (!data.isObject()) return log::error("Invalid profile data format");

        if (data.size() == 0) return;

        std::unordered_map<int, matjson::Value> dataValues;
        for (auto& [k, v] : data) {
            if (!v.isObject()) continue;
            if (auto id = jasmine::convert::get<int>(k)) {
                dataValues.emplace(id.value(), std::move(v));
            }
        }

        if constexpr (std::is_same_v<U, user_data::ProfileEvent>) {
            auto score = static_cast<GJUserScore*>(object);
            applyData<U>(score, score->m_accountID, dataValues);
        }
        else if constexpr (std::is_same_v<U, user_data::CommentEvent>) {
            for (auto comment : CCArrayExt<GJComment*>(static_cast<CCArray*>(object))) {
                if (auto score = comment->m_userScore) {
                    applyData<U>(comment, score->m_accountID, dataValues);
                }
            }
        }
        else if constexpr (std::is_same_v<U, user_data::ProfileCommentEvent>) {
            for (auto comment : CCArrayExt<GJComment*>(static_cast<CCArray*>(object))) {
                applyData<U>(comment, comment->m_accountID, dataValues);
            }
        }
        else {
            for (auto score : CCArrayExt<GJUserScore*>(static_cast<CCArray*>(object))) {
                applyData<U>(score, score->m_accountID, dataValues);
            }
        }
    });
}

#ifdef GEODE_IS_ANDROID
int scoreCompare(const void* a, const void* b);
int scoreCompareMoons(const void* a, const void* b);
int scoreCompareDemons(const void* a, const void* b);
int scoreCompareUserCoins(const void* a, const void* b);
#else
template <class F>
void* wrapFunction(uintptr_t address) {
    auto wrapped = hook::createWrapper(reinterpret_cast<void*>(base::get() + address), {
		.m_convention = hook::createConvention(tulip::hook::TulipConvention::Default),
		.m_abstract = tulip::hook::AbstractFunction::from(F(nullptr)),
	});
    if (wrapped.isErr()) {
        throw std::runtime_error(wrapped.unwrapErr());
    }
    return wrapped.unwrap();
}

int scoreCompare(const void* a, const void* b) {
    using FunctionType = int(*)(const void*, const void*);
    static auto func = wrapFunction<FunctionType>(GEODE_WINDOWS(0x142a00) GEODE_ARM_MAC(0x470e64) GEODE_INTEL_MAC(0x51a730) GEODE_IOS(0x8794c));
    return reinterpret_cast<FunctionType>(func)(a, b);
}

int scoreCompareMoons(const void* a, const void* b) {
    using FunctionType = int(*)(const void*, const void*);
    static auto func = wrapFunction<FunctionType>(GEODE_WINDOWS(0x142aa0) GEODE_ARM_MAC(0x470ee8) GEODE_INTEL_MAC(0x51a7c0) GEODE_IOS(0x879d0));
    return reinterpret_cast<FunctionType>(func)(a, b);
}

int scoreCompareDemons(const void* a, const void* b) {
    using FunctionType = int(*)(const void*, const void*);
    static auto func = wrapFunction<FunctionType>(GEODE_WINDOWS(0x142ad0) GEODE_ARM_MAC(0x470f0c) GEODE_INTEL_MAC(0x51a7f0) GEODE_IOS(0x879f4));
    return reinterpret_cast<FunctionType>(func)(a, b);
}

int scoreCompareUserCoins(const void* a, const void* b) {
    using FunctionType = int(*)(const void*, const void*);
    static auto func = wrapFunction<FunctionType>(GEODE_WINDOWS(0x142b00) GEODE_ARM_MAC(0x470f30) GEODE_INTEL_MAC(0x51a820) GEODE_IOS(0x87a18));
    return reinterpret_cast<FunctionType>(func)(a, b);
}
#endif

class $modify(UDAGameLevelManager, GameLevelManager) {
    static void onModify(ModifyBase<ModifyDerive<UDAGameLevelManager, GameLevelManager>>& self) {
        enabled = jasmine::gdps::isActive();
        if (!enabled) log::error("GDPS detected, User Data API disabled");

        for (auto& [name, hook] : self.m_hooks) {
            hook->setPriority(Priority::Replace);
            hook->setAutoEnable(enabled);
        }
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

        auto comments = createAndGetLevelComments(split[0], levelIDFromCommentKey(tag.c_str()));
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            if (auto score = comment->m_userScore) {
                comment->setUserFlag("downloading"_spr, true);
            }
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());

        fetchData<user_data::CommentEvent>(comments);
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

        auto scores = CCArray::create();
        auto status = type == UserListType::Blocked ? 2 : 1;
        for (auto& result : string::split(response, "|")) {
            auto score = GJUserScore::create(responseToDict(result, false));
            score->m_friendReqStatus = status;
            if (score->m_accountID > 0 && score->m_userID > 0) {
                score->setUserFlag("downloading"_spr, true);
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

        fetchData<user_data::FriendEvent>(scores);
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

        auto scores = CCArray::create();
        auto status = getSplitIntFromKey(tag.c_str(), 1) != 0 ? 4 : 3;
        for (auto& result : string::split(split[0], "|")) {
            auto dict = responseToDict(result, false);
            if (auto score = GJUserScore::create(dict)) {
                if (auto request = GJFriendRequest::create(dict)) {
                    score->m_friendReqStatus = status;
                    request->m_isRead = !score->m_newFriendRequest;
                    request->m_accountID = score->m_accountID;
                    score->setUserFlag("downloading"_spr, true);
                    scores->addObject(score);
                    storeFriendRequest(request);
                    storeUserName(score->m_userID, score->m_accountID, score->m_userName);
                }
            }
        }

        storeCommentsResult(scores, pageInfo, tag.c_str());
        if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFinished(scores, tag.c_str());

        fetchData<user_data::FriendRequestEvent>(scores);
    }

    void onGetLeaderboardScoresCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        auto type = LeaderboardType::Default;
        auto stat = LeaderboardStat::Stars;
        auto split = string::split(tag, "_");
        if (split.size() == 3) {
            type = (LeaderboardType)atoi(split[1].c_str());
            stat = (LeaderboardStat)atoi(split[2].c_str());
        }

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            return;
        }

        auto scores = createAndGetScores(response, type == LeaderboardType::Creator ? GJScoreType::Creator : GJScoreType::Top);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            score->setUserFlag("downloading"_spr, true);
            if (stat != LeaderboardStat::Stars) score->m_leaderboardStat = stat;
        }

        if (type == LeaderboardType::Friends) {
            auto scoresData = scores->data;
            switch (stat) {
                case LeaderboardStat::Moons:
                    qsort(scoresData->arr, scoresData->num, sizeof(GJUserScore*), scoreCompareMoons);
                    break;
                case LeaderboardStat::Demons:
                    qsort(scoresData->arr, scoresData->num, sizeof(GJUserScore*), scoreCompareDemons);
                    break;
                case LeaderboardStat::UserCoins:
                    qsort(scoresData->arr, scoresData->num, sizeof(GJUserScore*), scoreCompareUserCoins);
                    break;
                default:
                    qsort(scoresData->arr, scoresData->num, sizeof(GJUserScore*), scoreCompare);
                    break;
            }
            auto rank = 1;
            for (auto score : CCArrayExt<GJUserScore*>(scores)) {
                score->m_playerRank = rank++;
            }
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());

        fetchData<user_data::GlobalScoreEvent>(scores);
    }

    void onGetLevelLeaderboardCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        auto type = LevelLeaderboardType::Friends;
        auto mode = LevelLeaderboardMode::Time;
        if (!tag.empty()) {
            auto split = string::split(tag, "_");
            if (split.size() == 4) {
                type = (LevelLeaderboardType)atoi(split[2].c_str());
                mode = (LevelLeaderboardMode)atoi(split[3].c_str());
            }
        }

        auto responseNum = atoi(response.c_str());
        if (type == LevelLeaderboardType::Local) {
            auto level = static_cast<GJGameLevel*>(m_localLeaderboardLevels->objectForKey(tag));
            if (level && responseNum >= 0) {
                if (mode == LevelLeaderboardMode::Time) level->m_savedTime = false;
                else if (mode == LevelLeaderboardMode::Points) level->m_savedPoints = false;
            }
            m_localLeaderboardLevels->removeObjectForKey(tag);
            return;
        }
        else if (responseNum < 0) {
            if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            return;
        }

        auto scores = createAndGetScores(response, GJScoreType::LevelScore);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            score->setUserFlag("downloading"_spr, true);
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());

        fetchData<user_data::LevelScoreEvent>(scores);
    }

    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        auto accountID = getSplitIntFromKey(tag.c_str(), 1);
        if (response == "-1") {
            if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFailed(accountID);
            return;
        }

        auto dict = responseToDict(response, false);
        auto score = GJUserScore::create(dict);
        if (!score) {
            if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFailed(accountID);
            return;
        }

        score->setUserFlag("downloading"_spr, true);

        storeUserInfo(score);
        if (score->m_friendReqStatus == 3) {
            auto request = GJFriendRequest::create(dict);
            request->m_accountID = score->m_accountID;
            storeFriendRequest(request);
        }

        if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFinished(score);

        fetchData<user_data::ProfileEvent>(score);
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

        auto comments = createAndGetAccountComments(split[0], levelIDFromCommentKey(tag.c_str()));
        for (auto comment : CCArrayExt<GJComment*>(comments)) {
            comment->setUserFlag("downloading"_spr, true);
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());

        fetchData<user_data::ProfileCommentEvent>(comments);
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

        auto scores = createAndGetScores(split[0], GJScoreType::Search);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            score->setUserFlag("downloading"_spr, true);
        }

        storeSearchResult(scores, pageInfo, tag.c_str());
        if (m_levelManagerDelegate) m_levelManagerDelegate->loadLevelsFinished(scores, tag.c_str());

        fetchData<user_data::SearchResultEvent>(scores);
    }
};
