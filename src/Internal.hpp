#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/utils/web.hpp>

extern bool enabled;
extern std::unordered_map<std::string, matjson::Value> userData;
extern std::unordered_set<std::string> pendingKeys;

inline std::unordered_map<int, matjson::Value> prepareData(const std::string& key) {
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

template <std::derived_from<geode::Event> U, std::derived_from<cocos2d::CCNode> T>
inline void applyData(T* node, int id, const std::unordered_map<int, matjson::Value>& dataValues, bool pending) {
    if (auto it = dataValues.find(id); it != dataValues.end()) {
        for (auto& [k, v] : it->second) {
            node->setUserObject(fmt::format("{}"_spr, k), cocos2d::CCString::create(v.dump(0)));
        }
        U(node, it->first).post();
    }
    else if (pending) node->setUserObject("downloading"_spr, cocos2d::CCBool::create(true));
}

template <class U>
inline void fetchData(GameLevelManager* manager, const std::string& key, int id = 0) {
    using T = std::remove_pointer_t<geode::utils::function::Arg<0, typename U::Callback>>;
    pendingKeys.insert(key);
    geode::utils::web::WebRequest()
        .get(fmt::format("https://playerdata.hiimjasmine00.com/v3/data{}", id > 0 ? fmt::format("?ids={}", id) : ""))
        .listen([manager, id, key](geode::utils::web::WebResponse* res) {
            pendingKeys.erase(key);

            cocos2d::CCObject* object = nullptr;
            if (id > 0) {
                auto score = manager->userInfoForAccountID(id);
                if (score) score->setUserObject("downloading"_spr, nullptr);
                object = score;
            }
            else {
                auto objects = manager->getStoredOnlineLevels(key.c_str());
                for (auto node : geode::cocos::CCArrayExt<cocos2d::CCNode*>(objects)) {
                    node->setUserObject("downloading"_spr, nullptr);
                }
                object = objects;
            }

            if (!res->ok()) {
                auto data = res->data();
                return geode::log::error("Failed to get profile data: {}", std::string(data.begin(), data.end()));
            }

            if (auto json = res->json()) {
                auto data = json.unwrap();
                if (!data.isObject()) return geode::log::error("Invalid profile data format");

                if (manager->isDLActive(key.c_str())) userData[key] = data;
                else {
                    std::unordered_map<int, matjson::Value> dataValues;
                    for (auto& [k, v] : data) {
                        auto id = 0;
                        if (std::from_chars(k.data(), k.data() + k.size(), id).ec == std::errc()) dataValues[id] = v;
                    }

                    if (id > 0) {
                        if (auto score = static_cast<T*>(object)) applyData<typename U::Event, T>(score, id, dataValues, false);
                    }
                    else {
                        for (auto node : geode::cocos::CCArrayExt<cocos2d::CCNode*>(static_cast<cocos2d::CCArray*>(object))) {
                            auto id = 0;
                            if constexpr (std::is_same_v<T, GJComment>) {
                                auto comment = static_cast<GJComment*>(node);
                                id = comment->m_userScore ? comment->m_userScore->m_accountID : comment->m_accountID;
                            }
                            else if constexpr (std::is_same_v<T, GJUserScore>) {
                                id = static_cast<GJUserScore*>(node)->m_accountID;
                            }
                            if (id <= 0) continue;
                            applyData<typename U::Event, T>(static_cast<T*>(node), id, dataValues, false);
                        }
                    }
                }
            }
            else if (json.isErr()) {
                geode::log::error("Failed to parse profile data: {}", json.unwrapErr());
            }
        });
}
