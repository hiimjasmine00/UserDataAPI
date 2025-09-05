#include <cocos2d.h>
#include <Geode/GeneratedPredeclare.hpp>
#include <matjson.hpp>

namespace user_data::internal {
    extern std::unordered_map<std::string, matjson::Value> userData;
    extern std::unordered_set<std::string> pendingKeys;

    typedef void(*SingleResponseCallback)(cocos2d::CCNode*, const matjson::Value&);
    typedef void(*MultipleResponseCallback)(cocos2d::CCArray*, const matjson::Value&);

    void applyData(cocos2d::CCNode* object, const std::string& key, SingleResponseCallback callback);
    void applyData(cocos2d::CCArray* objects, const std::string& key, MultipleResponseCallback callback);
    void fetchData(GameLevelManager* manager, int id, const std::string& key, SingleResponseCallback callback);
    void fetchData(GameLevelManager* manager, const std::string& key, MultipleResponseCallback callback);
}
