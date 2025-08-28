#pragma once
#include <cocos2d.h>
#include <fmt/format.h>
#ifdef USER_DATA_API_EVENTS
#include <Geode/loader/Dispatch.hpp>
#endif
#include <matjson.hpp>

#ifndef USER_DATA_API_EVENTS
    #ifdef GEODE_IS_WINDOWS
        #ifdef USER_DATA_API_EXPORTING
            #define USER_DATA_API_DLL __declspec(dllexport)
        #else
            #define USER_DATA_API_DLL __declspec(dllimport)
        #endif
    #else
        #ifdef USER_DATA_API_EXPORTING
            #define USER_DATA_API_DLL __attribute__((visibility("default")))
        #else
            #define USER_DATA_API_DLL
        #endif
    #endif
#else
    #define USER_DATA_API_DLL
#endif

namespace user_data {
    constexpr std::string_view ID = "hiimjasmine00.user_data_api";

    /// Returns whether the given node contains user data for the specified ID.
    /// @param node The node to check
    /// @param id The ID to check for (default is your mod ID)
    /// @returns True if the node contains user data for the specified ID, false otherwise.
    inline bool contains(cocos2d::CCNode* node, std::string_view id = GEODE_MOD_ID) {
        return node->getUserObject(fmt::format("{}/{}", ID, id)) != nullptr;
    }

    /// Returns whether the given node is currently downloading user data.
    /// @param node The node to check
    /// @returns True if the node is currently downloading user data, false otherwise.
    inline bool downloading(cocos2d::CCNode* node) {
        return node->getUserObject(fmt::format("{}/downloading", ID)) != nullptr;
    }

    /// Retrieves user data from the given node for the specified ID and converts it to the specified type. (Defaults to matjson::Value)
    /// @param node The node to retrieve data from
    /// @param id The ID to retrieve data for (Defaults to your mod ID)
    /// @returns A Result containing the user data as the specified type, or an error if the data could not be retrieved or converted.
    template <class T = matjson::Value>
    inline geode::Result<T> get(cocos2d::CCNode* node, std::string_view id = GEODE_MOD_ID) {
        if (auto obj = static_cast<cocos2d::CCString*>(node->getUserObject(fmt::format("{}/{}", ID, id)))) {
            return matjson::parseAs<T>(obj->m_sString);
        }
        else {
            return geode::Err("User object not found");
        }
    }

    /// Uploads user data to the User Data API.
    /// @param data The user data to upload as a matjson::Value
    /// @param id The ID to upload data for (Defaults to your mod ID)
    void USER_DATA_API_DLL upload(const matjson::Value& data, std::string_view id = GEODE_MOD_ID);
};

#ifdef USER_DATA_API_EVENTS
inline void user_data::upload(const matjson::Value& data, std::string_view id) {
    geode::DispatchEvent<matjson::Value, std::string>(fmt::format("{}/v1/upload", ID), data, std::string(id)).post();
}
#endif
