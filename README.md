# Player Data API
An API for managing extra profile data.

## Usage
To use Player Data API, you need to include it as a dependency in your Geode mod. You can do this by adding the following lines to your `mod.json` file:
```json
{
    "dependencies": {
        "hiimjasmine00.player_data_api": {
            "version": ">=v1.0.0",
            "importance": "required"
        }
    }
}
```
If what you are doing requires the mod, then use `"required"` as the importance. If it is optional, use `"recommended"` (Installs the mod but does not require it) or `"suggested"` (Does not install the mod, but suggests it to the user).

For actual usage of the API, here is an example:
```cpp
// Include the Player Data API header
// If you are using "recommended" or "suggested" importance, you should define the macro `PLAYER_DATA_API_EVENTS` before including the header
#include <hiimjasmine00.player_data_api/include/PlayerDataAPI.hpp>

using namespace geode::prelude;

// Uploading data
PlayerDataAPI::upload(0);
PlayerDataAPI::upload("Hello, world!");
PlayerDataAPI::upload(matjson::makeObject({ { "hello", "world" } }));

// Global event listeners
$on_mod(Loaded) {
    new EventListener(+[](GJUserScore* score) {
        log::info("{}", PlayerDataAPI::get<std::string>(score).unwrapOr("")); // Leave template empty for a matjson::Value
        return ListenerResult::Propagate;
    }, PlayerDataAPIGetFilter(nullptr, 0)); // Replace "0" with the account ID you want to filter by
}

// Node-based event listeners
#include <Geode/modify/ProfilePage.hpp>
class $modify(ProfilePage) {
    bool init(int accountID, bool ownProfile) {
        if (!ProfilePage::init(accountID, ownProfile)) return false;

        // Add an event listener for the PlayerDataAPIGetFilter
        addEventListener<PlayerDataAPIGetFilter>([this](GJUserScore* score) {
            doStuffWithScore(score);
            return ListenerResult::Propagate;
        }, accountID);

        return true;
    }

    void doStuffWithScore(GJUserScore* score) {
        log::info("{}", PlayerDataAPI::get<std::string>(score).unwrapOr("")); // Leave template empty for a matjson::Value
    }
};
```
Full documentation can be found [here](https://github.com/hiimjasmine00/PlayerDataAPI/blob/main/include/PlayerDataAPI.hpp).

## License
This mod is licensed under the [MIT License](./LICENSE).