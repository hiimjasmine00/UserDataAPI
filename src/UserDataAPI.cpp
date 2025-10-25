#include <argon/argon.hpp>
#include <Events.hpp>
#include <Geode/binding/FriendRequestDelegate.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GJFriendRequest.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/binding/LevelCommentDelegate.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/binding/UserInfoDelegate.hpp>
#include <Geode/binding/UserListDelegate.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <UserDataAPI.hpp>

using namespace geode::prelude;

bool enabled = true;
std::vector<std::string> mods;

$on_mod(Loaded) {
    argon::startAuth().listen([](Result<std::string>* res) {
        if (res->isErr()) log::error("Argon authentication failed: {}", res->unwrapErr());
    });

    new EventListener([](const matjson::Value& data, const std::string& id) {
        user_data::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<matjson::Value, std::string>("v1/upload"_spr));

    new EventListener(+[](GameEvent*) {
        for (auto mod : Loader::get()->getAllMods()) {
            if (mod->isEnabled() && std::ranges::contains(mod->getMetadataRef().getDependencies(), GEODE_MOD_ID, &ModMetadata::Dependency::id)) {
                auto modID = mod->getID();
                if (modID == "camila314.comment-colors") modID = "camila314.comment-color";
                mods.push_back(modID);
            }
        }
    }, GameEventFilter(GameEventType::Loaded));
}

void user_data::upload(const matjson::Value& data, std::string_view id) {
    if (!enabled) return;

    argon::startAuth().listen([data, id = std::string(id)](Result<std::string>* res) {
        if (res->isOk()) {
            web::WebRequest()
                .bodyJSON(data)
                .header("Authorization", res->unwrap())
                .post(fmt::format("https://userdataapi.dankmeme.dev/v1/upload?id={}&mod={}",
                    GJAccountManager::get()->m_accountID, id))
                .listen([](web::WebResponse* res) {
                    if (!res->ok()) log::error("Failed to upload user data: {}", std::string(std::from_range, res->data()));
                });
        }
        else if (res->isErr()) log::error("Argon authentication failed: {}", res->unwrapErr());
    });
}

template <std::derived_from<Event> U, std::derived_from<CCNode> T>
void applyData(T* node, int id, const std::unordered_map<int, matjson::Value>& dataValues) {
    node->setUserObject("downloading"_spr, nullptr);
    if (auto it = dataValues.find(id); it != dataValues.end()) {
        for (auto& [k, v] : it->second) {
            node->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
        }
        U(node, id).post();
    }
}

template <class U>
void fetchData(CCObject* object) {
    using T = std::remove_pointer_t<function::Arg<0, typename U::Callback>>;

    if (!object) return;
    if constexpr (!std::is_same_v<U, user_data::ProfileFilter>) {
        if (static_cast<CCArray*>(object)->count() == 0) return;
    }

    std::string url;
    if constexpr (std::is_same_v<U, user_data::ProfileFilter>) {
        url = fmt::format("https://userdataapi.dankmeme.dev/v1/data?players={}&mods={}",
            static_cast<GJUserScore*>(object)->m_accountID, fmt::join(mods, ","));
    }
    else {
        std::set<int> ids;
        for (auto obj : CCArrayExt<CCNode*>(static_cast<CCArray*>(object))) {
            if constexpr (std::is_same_v<U, user_data::CommentFilter>) {
                if (auto score = static_cast<GJComment*>(obj)->m_userScore) ids.insert(score->m_accountID);
            }
            else if constexpr (std::is_same_v<U, user_data::ProfileCommentFilter>) {
                ids.insert(static_cast<GJComment*>(obj)->m_accountID);
            }
            else {
                ids.insert(static_cast<GJUserScore*>(obj)->m_accountID);
            }
        }
        url = fmt::format("https://userdataapi.dankmeme.dev/v1/data?players={}&mods={}", fmt::join(ids, ","), fmt::join(mods, ","));
    }

    web::WebRequest().get(url).listen([objectRef = WeakRef(object)](web::WebResponse* res) {
        if (!res->ok()) return log::error("Failed to get profile data: {}", std::string(std::from_range, res->data()));

        auto json = res->json();
        if (json.isErr()) return log::error("Failed to parse profile data: {}", json.unwrapErr());

        auto data = json.unwrap();
        if (!data.isObject()) return log::error("Invalid profile data format");

        if (data.size() == 0) return;

        auto object = objectRef.lock().data();
        if (!object) return;

        std::unordered_map<int, matjson::Value> dataValues;
        for (auto& [k, v] : data) {
            auto id = 0;
            if (std::from_chars(k.data(), k.data() + k.size(), id).ec == std::errc()) dataValues[id] = v;
        }

        if constexpr (std::is_same_v<U, user_data::ProfileFilter>) {
            auto score = static_cast<GJUserScore*>(object);
            applyData<typename U::Event>(score, score->m_accountID, dataValues);
        }
        else if constexpr (std::is_same_v<U, user_data::CommentFilter>) {
            for (auto comment : CCArrayExt<GJComment*>(static_cast<CCArray*>(object))) {
                if (auto score = comment->m_userScore) {
                    applyData<typename U::Event>(comment, score->m_accountID, dataValues);
                }
            }
        }
        else if constexpr (std::is_same_v<U, user_data::ProfileCommentFilter>) {
            for (auto comment : CCArrayExt<GJComment*>(static_cast<CCArray*>(object))) {
                applyData<typename U::Event>(comment, comment->m_accountID, dataValues);
            }
        }
        else {
            for (auto score : CCArrayExt<GJUserScore*>(static_cast<CCArray*>(object))) {
                applyData<typename U::Event>(score, score->m_accountID, dataValues);
            }
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
                comment->setUserObject("downloading"_spr, CCBool::create(true));
            }
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());

        fetchData<user_data::CommentFilter>(comments);
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
                score->setUserObject("downloading"_spr, CCBool::create(true));
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

        fetchData<user_data::FriendFilter>(scores);
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
                    score->setUserObject("downloading"_spr, CCBool::create(true));
                    scores->addObject(score);
                    storeFriendRequest(request);
                    storeUserName(score->m_userID, score->m_accountID, score->m_userName);
                }
            }
        }

        storeCommentsResult(scores, pageInfo, tag.c_str());
        if (m_friendRequestDelegate) m_friendRequestDelegate->loadFRequestsFinished(scores, tag.c_str());

        fetchData<user_data::FriendRequestFilter>(scores);
    }

    void onGetLeaderboardScoresCompleted(gd::string response, gd::string tag) {
        removeDLFromActive(tag.c_str());

        if (response == "-1") {
            if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFailed(tag.c_str());
            return;
        }

        auto scores = createAndGetScores(response, tag == "leaderboard_creator" ? GJScoreType::Creator : GJScoreType::Top);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            score->setUserObject("downloading"_spr, CCBool::create(true));
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

        fetchData<user_data::GlobalScoreFilter>(scores);
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

        auto scores = createAndGetScores(response, GJScoreType::LevelScore);
        for (auto score : CCArrayExt<GJUserScore*>(scores)) {
            score->setUserObject("downloading"_spr, CCBool::create(true));
        }

        storeSearchResult(scores, " ", tag.c_str());
        if (m_leaderboardManagerDelegate) m_leaderboardManagerDelegate->loadLeaderboardFinished(scores, tag.c_str());

        fetchData<user_data::LevelScoreFilter>(scores);
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

        auto dict = responseToDict(response, false);
        auto score = GJUserScore::create(dict);
        if (!score) {
            if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFailed(accountID);
            return;
        }

        score->setUserObject("downloading"_spr, CCBool::create(true));

        storeUserInfo(score);
        if (score->m_friendReqStatus == 3) {
            auto request = GJFriendRequest::create(dict);
            request->m_accountID = score->m_accountID;
            storeFriendRequest(request);
        }

        if (m_userInfoDelegate) m_userInfoDelegate->getUserInfoFinished(score);

        fetchData<user_data::ProfileFilter>(score);
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
            comment->setUserObject("downloading"_spr, CCBool::create(true));
        }

        storeCommentsResult(comments, pageInfo, tag.c_str());
        if (m_levelCommentDelegate) m_levelCommentDelegate->loadCommentsFinished(comments, tag.c_str());

        fetchData<user_data::ProfileCommentFilter>(comments);
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
            score->setUserObject("downloading"_spr, CCBool::create(true));
        }

        storeSearchResult(scores, pageInfo, tag.c_str());
        if (m_levelManagerDelegate) m_levelManagerDelegate->loadLevelsFinished(scores, tag.c_str());

        fetchData<user_data::SearchResultFilter>(scores);
    }
};
