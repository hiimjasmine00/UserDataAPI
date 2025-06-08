#include <argon/argon.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/UserInfoDelegate.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/utils/web.hpp>
#include <PlayerDataAPI.hpp>

using namespace geode::prelude;

#define PLAYER_DATA_URL "https://playerdata.hiimjasmine00.com/v1/{}"

$on_mod(Loaded) {
    (void)argon::startAuth([](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());
    }).inspectErr([](const std::string& err) {
        log::error("Failed to start Argon authentication: {}", err);
    });

    new EventListener(+[](const std::string& id, const matjson::Value& data) {
        PlayerDataAPI::upload(data, id);
        return ListenerResult::Propagate;
    }, DispatchFilter<std::string, matjson::Value>("v1/upload"_spr));
}

void applyData(GJUserScore* score, const matjson::Value& data) {
    auto mod = Mod::get();
    for (auto& [key, value] : data) {
        score->setUserObject(std::string(mod->expandSpriteName(key)), CCString::create(value.dump(0)));
    }
    PlayerDataAPIGetEvent(score).post();
}

void PlayerDataAPI::upload(const matjson::Value& data, std::string_view id) {
    (void)argon::startAuth([data, id](Result<std::string> res) {
        if (res.isErr()) log::error("Argon authentication failed: {}", res.unwrapErr());

        web::WebRequest()
            .bodyJSON(data)
            .header("Authorization", res.unwrap())
            .post(fmt::format(PLAYER_DATA_URL "/{}", GJAccountManager::get()->m_accountID, id))
            .listen([](web::WebResponse* res) {
                if (!res->ok()) log::error("Failed to upload player data: {}", res->string().unwrapOr(""));
            });
    }).inspectErr([](const std::string& err) {
        log::error("Failed to start Argon authentication: {}", err);
    });
}

class $modify(PDAGameLevelManager, GameLevelManager) {
    struct Fields : UserInfoDelegate {
        std::unordered_map<int, matjson::Value> m_userData;
        UserInfoDelegate* m_delegate = nullptr;

        void getUserInfoFinished(GJUserScore* score) override {
            auto accountID = score->m_accountID;
            if (m_userData.contains(accountID)) {
                applyData(score, m_userData[accountID]);
                m_userData.erase(accountID);
            }
            GameLevelManager::get()->m_userInfoDelegate = m_delegate;
            if (m_delegate) m_delegate->getUserInfoFinished(score);
        }

        void getUserInfoFailed(int accountID) override {
            GameLevelManager::get()->m_userInfoDelegate = m_delegate;
            if (m_delegate) m_delegate->getUserInfoFailed(accountID);
        }

        void userInfoChanged(GJUserScore* score) override {
            GameLevelManager::get()->m_userInfoDelegate = m_delegate;
            if (m_delegate) m_delegate->userInfoChanged(score);
        }
    };

    void getGJUserInfo(int accountID) {
        GameLevelManager::getGJUserInfo(accountID);

        web::WebRequest().get(fmt::format(PLAYER_DATA_URL, accountID)).listen([this, accountID](web::WebResponse* res) {
            if (!res->ok()) return log::error("Failed to get player data: {}", res->string().unwrapOr(""));

            auto data = res->json().unwrapOr(matjson::Value::object());
            if (!data.isObject()) return log::error("Invalid player data format");

            queueInMainThread([this, accountID, data] {
                if (isDLActive(fmt::format("account_{}", accountID).c_str())) m_fields->m_userData[accountID] = data;
                else if (auto score = userInfoForAccountID(accountID)) applyData(score, data);
            });
        });
    }

    void onGetGJUserInfoCompleted(gd::string response, gd::string tag) {
        auto f = m_fields.self();
        if (response != "-1") {
            auto split = string::split(tag, "_");
            if (split.size() >= 2) {
                auto accountID = numFromString<int>(split[1]).unwrapOr(0);
                if (accountID > 0) {
                    if (f->m_userData.contains(accountID)) {
                        f->m_delegate = m_userInfoDelegate;
                        m_userInfoDelegate = f;
                    }
                }
                else log::error("Invalid account ID: {}", split[1]);
            }
            else log::error("Invalid tag: {}", GEODE_ANDROID(std::string)(tag));
        }

        GameLevelManager::onGetGJUserInfoCompleted(response, tag);
    }
};
