#include "Internal.hpp"
#include <argon/argon.hpp>
#include <Events.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#ifdef GEODE_IS_ANDROID
#include <Geode/binding/GJMoreGamesLayer.hpp>
#endif
#include <Geode/loader/Dispatch.hpp>
#include <Geode/loader/Mod.hpp>
#include <km7dev.server_api/include/ServerAPIEvents.hpp>
#include <UserDataAPI.hpp>

using namespace geode::prelude;

bool enabled = true;
std::unordered_map<std::string, matjson::Value> userData;
std::unordered_set<std::string> pendingKeys;

$execute {
    std::string_view url;
    auto foundURL = false;
    if (Loader::get()->isModLoaded("km7dev.server_api")) {
        url = ServerAPIEvents::getCurrentServer().url;
        if (!url.empty() && url != "NONE_REGISTERED") {
            for (; url.ends_with("/"); url = url.substr(0, url.size() - 1));
            foundURL = true;
        }
    }

    if (!foundURL) {
        url = reinterpret_cast<const char*>(base::get() +
            GEODE_WINDOWS(0x53ea48)
            GEODE_ARM_MAC(0x7749fb)
            GEODE_INTEL_MAC(0x8516bf)
            GEODE_ANDROID64((((GJMoreGamesLayer* volatile)nullptr)->getMoreGamesList()->count() == 0 ? 0xea27f8 : 0xea2988))
            GEODE_ANDROID32((((GJMoreGamesLayer* volatile)nullptr)->getMoreGamesList()->count() == 0 ? 0x952cce : 0x952e9e))
            GEODE_IOS(0x6af51a));
        if (url.size() > 34) url = url.substr(0, 34);
    }

    enabled = url.ends_with("://www.boomlings.com/database");
    if (!enabled) log::error("GDPS detected, User Data API disabled");
}

$on_mod(Loaded) {
    auto res = argon::startAuth([](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    });
    if (res.isErr()) log::error("Failed to start Argon authentication: {}", res.unwrapErr());

    new EventListener([](const matjson::Value& data, const std::string& id) {
        user_data::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<matjson::Value, std::string>("/v1/upload"_spr));
}

void user_data::upload(const matjson::Value& data, std::string_view id) {
    if (!enabled) return;

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
