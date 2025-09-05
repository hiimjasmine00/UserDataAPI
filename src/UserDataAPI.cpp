#include "Internal.hpp"
#include <argon/argon.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/utils/cocos.hpp>
#include <UserDataAPI.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    auto res = argon::startAuth([](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    });
    if (res.isErr()) log::error("Failed to start Argon authentication: {}", res.unwrapErr());

    new EventListener([](const matjson::Value& data, const std::string& id) {
        user_data::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<matjson::Value, std::string>(GEODE_MOD_ID "/v1/upload"));
}

void user_data::upload(const matjson::Value& data, std::string_view id) {
    auto res = argon::startAuth([data, id](Result<std::string> res) {
        if (res.isOk()) {
            web::WebRequest()
                .bodyJSON(data)
                .header("Authorization", res.unwrap())
                .post(fmt::format("https://playerdata.hiimjasmine00.com/v3/upload?id={}&mod={}", GJAccountManager::get()->m_accountID, id))
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

std::unordered_map<std::string, matjson::Value> user_data::internal::userData;
std::unordered_set<std::string> user_data::internal::pendingKeys;

void user_data::internal::applyData(CCNode* object, const std::string& key, SingleResponseCallback callback) {
    if (auto it = pendingKeys.find(key); it != pendingKeys.end()) {
        object->setUserObject(GEODE_MOD_ID "/downloading", CCBool::create(true));
        pendingKeys.erase(it);
    }
    if (auto it = userData.find(key); it != userData.end()) {
        callback(object, it->second);
        userData.erase(it);
    }
}

void user_data::internal::applyData(CCArray* objects, const std::string& key, MultipleResponseCallback callback) {
    if (auto it = pendingKeys.find(key); it != pendingKeys.end()) {
        for (auto object : CCArrayExt<CCNode*>(objects)) {
            object->setUserObject(GEODE_MOD_ID "/downloading", CCBool::create(true));
        }
        pendingKeys.erase(it);
    }
    if (auto it = userData.find(key); it != userData.end()) {
        callback(objects, it->second);
        userData.erase(it);
    }
}

void user_data::internal::fetchData(GameLevelManager* manager, int id, const std::string& key, SingleResponseCallback callback) {
    pendingKeys.insert(key);
    web::WebRequest().get("https://playerdata.hiimjasmine00.com/v3/data").listen([manager, id, key, callback](web::WebResponse* res) {
        pendingKeys.erase(key);

        auto object = static_cast<CCNode*>(manager->m_storedUserInfo->objectForKey(id));
        if (object) object->setUserObject(GEODE_MOD_ID "/downloading", CCBool::create(true));

        if (!res->ok()) {
            auto data = res->data();
            return log::error("Failed to get profile data: {}", std::string(data.begin(), data.end()));
        }

        if (auto json = res->json()) {
            auto data = json.unwrap();
            if (!data.isObject()) return log::error("Invalid profile data format");

            if (manager->m_downloadObjects->objectForKey(key)) userData[key] = data;
            else if (object) callback(object, data);
        }
        else if (json.isErr()) {
            log::error("Failed to parse profile data: {}", json.unwrapErr());
        }
    });
}

void user_data::internal::fetchData(GameLevelManager* manager, const std::string& key, MultipleResponseCallback callback) {
    pendingKeys.insert(key);
    web::WebRequest().get("https://playerdata.hiimjasmine00.com/v3/data").listen([manager, key, callback](web::WebResponse* res) {
        pendingKeys.erase(key);

        auto objects = static_cast<CCArray*>(manager->m_storedLevels->objectForKey(key));
        if (objects) {
            for (auto obj : CCArrayExt<CCNode*>(objects)) {
                obj->setUserObject(GEODE_MOD_ID "/downloading", nullptr);
            }
        }

        if (!res->ok()) {
            auto data = res->data();
            return log::error("Failed to get profile data: {}", std::string(data.begin(), data.end()));
        }

        if (auto json = res->json()) {
            auto data = json.unwrap();
            if (!data.isObject()) return log::error("Invalid profile data format");

            if (manager->m_downloadObjects->objectForKey(key)) userData[key] = data;
            else if (objects) callback(objects, data);
        }
        else if (json.isErr()) {
            log::error("Failed to parse profile data: {}", json.unwrapErr());
        }
    });
}
