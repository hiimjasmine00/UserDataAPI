#pragma once
#include <cocos2d.h>
#include <Geode/binding/GJUserScore.hpp>
#ifdef PLAYER_DATA_API_EVENTS
#include <Geode/loader/Dispatch.hpp>
#endif
#include <matjson.hpp>

#ifndef PLAYER_DATA_API_EVENTS
    #ifdef GEODE_IS_WINDOWS
        #ifdef PLAYER_DATA_API_EXPORTING
            #define PLAYER_DATA_API_DLL __declspec(dllexport)
        #else
            #define PLAYER_DATA_API_DLL __declspec(dllimport)
        #endif
    #else
        #define PLAYER_DATA_API_DLL __attribute__((visibility("default")))
    #endif
#else
    #define PLAYER_DATA_API_DLL
#endif

/// The class for interacting with the Player Data API.
class PlayerDataAPI {
public:
    /// Returns whether the given user score contains profile data for the specified ID.
    /// @param score The user score to check
    /// @param id The ID to check for (default is your mod ID)
    /// @returns True if the user score contains profile data for the specified ID, false otherwise.
    static bool contains(GJUserScore* score, std::string_view id = GEODE_MOD_ID) {
        return score->getUserObject(fmt::format("hiimjasmine00.player_data_api/{}", id)) != nullptr;
    }

    /// Retrieves profile data from the given user score for the specified ID.
    /// @param score The user score to retrieve data from
    /// @param id The ID to retrieve data for (Defaults to your mod ID)
    /// @returns A Result containing the profile data as a matjson::Value, or an error if the data could not be retrieved.
    static geode::Result<matjson::Value> get(GJUserScore* score, std::string_view id = GEODE_MOD_ID) {
        auto obj = score->getUserObject(fmt::format("hiimjasmine00.player_data_api/{}", id));
        if (!obj) return geode::Err("User object not found");

        GEODE_UNWRAP_INTO(auto ret, matjson::parse(static_cast<cocos2d::CCString*>(obj)->m_sString));
        return geode::Ok(ret);
    }

    /// Retrieves profile data from the given user score for the specified ID and converts it to the specified type.
    /// @param score The user score to retrieve data from
    /// @param id The ID to retrieve data for (Defaults to your mod ID)
    /// @returns A Result containing the profile data as the specified type, or an error if the data could not be retrieved or converted.
    template <class T>
    static geode::Result<T> get(GJUserScore* score, std::string_view id = GEODE_MOD_ID) {
        GEODE_UNWRAP_INTO(T ret, get(score, id).andThen([](const matjson::Value& val) { return val.as<T>(); }));
        return geode::Ok(ret);
    }

    /// Uploads profile data to the Player Data API.
    /// @param data The profile data to upload as a matjson::Value
    /// @param id The ID to upload data for (Defaults to your mod ID)
    static void PLAYER_DATA_API_DLL upload(const matjson::Value& data, std::string_view id = GEODE_MOD_ID);
};

#ifdef PLAYER_DATA_API_EVENTS
inline void PlayerDataAPI::upload(const matjson::Value& data, std::string_view id) {
    geode::DispatchEvent<matjson::Value, std::string>("hiimjasmine00.player_data_api/v1/upload", data, std::string(id)).post();
}
#endif

/// The event for when profile data is retrieved from the Player Data API.
class PlayerDataAPIGetEvent : public geode::Event {
protected:
    GJUserScore* m_score;
public:
    PlayerDataAPIGetEvent(GJUserScore* score) : m_score(score) {}

    /// Returns the user score associated with this event.
    /// @returns A pointer to the GJUserScore object associated with this event.
    GJUserScore* getScore() const { return m_score; }
};

/// The filter for PlayerDataAPIGetEvent.
class PlayerDataAPIGetFilter : public geode::EventFilter<PlayerDataAPIGetEvent> {
protected:
    int m_accountID;
public:
    using Callback = geode::ListenerResult(GJUserScore*);

    PlayerDataAPIGetFilter(cocos2d::CCNode* target, int accountID) : m_accountID(accountID) {}

    /// Handles the PlayerDataAPIGetEvent and invokes the provided callback function if the account ID matches.
    /// @param fn The callback function to invoke if the account ID matches
    /// @param event The PlayerDataAPIGetEvent to handle
    /// @returns ListenerResult::Propagate if the account ID does not match, otherwise invokes the callback function.
    geode::ListenerResult handle(std::function<Callback> fn, PlayerDataAPIGetEvent* event) {
        auto score = event->getScore();
        if (score->m_accountID == m_accountID) return fn(score);
        return geode::ListenerResult::Propagate;
    }
};
