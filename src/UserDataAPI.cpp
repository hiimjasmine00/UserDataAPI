#include <argon/argon.hpp>
#include <Events.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <UserDataAPI.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    argon::startAuth([](Result<std::string> res) {
        res.inspectErr([](const std::string& err) {
            log::error("Argon authentication failed: {}", err);
        });
    }).inspectErr([](const std::string& err) {
        log::error("Failed to start Argon authentication: {}", err);
    });
    new EventListener([](const matjson::Value& data, const std::string& id) {
        user_data::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<matjson::Value, std::string>("v1/upload"_spr));
}

void user_data::upload(const matjson::Value& data, std::string_view id) {
    argon::startAuth([data, id](Result<std::string> res) {
        res.inspect([&data, &id](const std::string& token) {
            web::WebRequest()
                .bodyJSON(data)
                .header("Authorization", token)
                .post(fmt::format("https://playerdata.hiimjasmine00.com/v1/{}/{}", GJAccountManager::get()->m_accountID, id))
                .listen([](web::WebResponse* res) {
                    if (!res->ok()) log::error("Failed to upload user data: {}", res->string().unwrapOrDefault());
                });
        }).inspectErr([](const std::string& err) {
            log::error("Argon authentication failed: {}", err);
        });
    }).inspectErr([](const std::string& err) {
        log::error("Failed to start Argon authentication: {}", err);
    });
}

class $modify(UDAGameLevelManager, GameLevelManager) {
    void handleIt(bool success, gd::string response, gd::string tag, GJHttpType type) {
        GameLevelManager::handleIt(success, response, tag, type);

        switch (type) {
            case GJHttpType::GetLeaderboardScores:
                if (success && response != "-1") {
                    handleResponse<user_data::GlobalScoreEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            case GJHttpType::GetLevelComments:
                if (success && response != "-1" && string::count(response, '#') > 0) {
                    handleResponse<user_data::CommentEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            case GJHttpType::GetGJUserInfo:
                if (success && response != "-1") {
                    handleResponse<user_data::ProfileEvent>(m_storedUserInfo->objectForKey(getSplitIntFromKey(tag.c_str(), 1)));
                }
                break;
            case GJHttpType::GetFriendRequests:
                if (success && response != "-1" && atoi(response.c_str()) > -1 && string::count(response, '#') > 0) {
                    handleResponse<user_data::FriendRequestEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            case GJHttpType::GetUserList:
                if (success && response != "-1" && atoi(response.c_str()) > -1) {
                    handleResponse<user_data::FriendEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            case GJHttpType::GetUsers:
                if (success && response != "-1" && string::count(response, '#') > 0) {
                    handleResponse<user_data::SearchResultEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            case GJHttpType::GetLevelLeaderboard:
                if (success && response != "-1") {
                    handleResponse<user_data::LevelScoreEvent>(m_storedLevels->objectForKey(tag));
                }
                break;
            default:
                break;
        }
    }

    template <std::derived_from<Event> T>
    void applyData(CCNode* node, const matjson::Value& data, int id) {
        data.get(fmt::to_string(id)).inspect([node, id](const matjson::Value& data) {
            for (auto& [k, v] : data) {
                node->setUserObject(fmt::format("{}"_spr, k), CCString::create(v.dump(0)));
            }
            if constexpr (std::is_same_v<T, user_data::CommentEvent>) T(static_cast<GJComment*>(node), id).post();
            else T(static_cast<GJUserScore*>(node), id).post();
        });
    }

    template <std::derived_from<Event> T>
    void handleResponse(CCObject* object) {
        if (!object) return;
        if constexpr (!std::is_same_v<T, user_data::ProfileEvent>) {
            if (static_cast<CCArray*>(object)->count() == 0) return;
        }

        std::string url;
        if constexpr (std::is_same_v<T, user_data::ProfileEvent>) {
            auto score = static_cast<GJUserScore*>(object);
            url = fmt::format("https://playerdata.hiimjasmine00.com/v2/{}", score->m_accountID);
            score->setUserObject("downloading"_spr, CCBool::create(true));
        }
        else {
            std::set<int> ids;
            for (auto obj : CCArrayExt<CCNode*>(static_cast<CCArray*>(object))) {
                if constexpr (std::is_same_v<T, user_data::CommentEvent>) {
                    if (auto score = static_cast<GJComment*>(obj)->m_userScore) ids.insert(score->m_accountID);
                }
                else {
                    ids.insert(static_cast<GJUserScore*>(obj)->m_accountID);
                }
                obj->setUserObject("downloading"_spr, CCBool::create(true));
            }
            url = fmt::format("https://playerdata.hiimjasmine00.com/v2/{}", fmt::join(ids, ","));
        }

        web::WebRequest().get(url).listen([this, objectRef = WeakRef(object)](web::WebResponse* res) {
            auto data = matjson::Value::object();
            auto response = res->string().unwrapOrDefault();

            if (res->ok()) {
                GEODE_UNWRAP_INTO_IF_OK(data, matjson::parse(response).inspectErr([](const matjson::ParseError& err) {
                    log::error("Failed to parse user data JSON: {}", err);
                }));
                if (!data.isObject()) log::error("Invalid user data format");
            }
            else log::error("Failed to get user data: {}", response);

            if (auto object = objectRef.lock().data()) {
                auto hasData = data.isObject() && data.size() > 0;
                if constexpr (std::is_same_v<T, user_data::ProfileEvent>) {
                    auto score = static_cast<GJUserScore*>(object);
                    score->setUserObject("downloading"_spr, nullptr);
                    if (hasData) applyData<T>(score, data, score->m_accountID);
                }
                else if constexpr (std::is_same_v<T, user_data::CommentEvent>) {
                    for (auto comment : CCArrayExt<GJComment*>(static_cast<CCArray*>(object))) {
                        comment->setUserObject("downloading"_spr, nullptr);
                        if (hasData) {
                            auto score = comment->m_userScore;
                            if (score) applyData<T>(comment, data, score->m_accountID);
                        }
                    }
                }
                else {
                    for (auto score : CCArrayExt<GJUserScore*>(static_cast<CCArray*>(object))) {
                        score->setUserObject("downloading"_spr, nullptr);
                        if (hasData) applyData<T>(score, data, score->m_accountID);
                    }
                }
            }
        });
    }
};
