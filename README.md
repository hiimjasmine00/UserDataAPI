# User Data API
An API for managing extra user data.

## Features
- Storing and retrieving extra user data from mods
- Uploading user data to the API
- Listening to events for user profiles, comments, friends, friend requests, and leaderboards

## Usage
To use User Data API, include it as a [dependency](https://docs.geode-sdk.org/mods/dependencies/) in your Geode mod by adding the following to your `mod.json`:
```json
{
    "dependencies": {
        "hiimjasmine00.user_data_api": {
            "version": ">=v1.0.0",
            "importance": "required"
        }
    }
}
```

### Dependency Importance Levels
- `required`: Mod **must** be installed for yours to run.
- `recommended`: Installed automatically, but your mod can still run without it.
- `suggested`: Not installed automatically.

Then, include the headers in your code.

**Required importance:**
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
#include <hiimjasmine00.user_data_api/include/UserDataAPI.hpp>
```

**Recommended/Suggested importance:**
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
#define USER_DATA_API_EVENTS
#include <hiimjasmine00.user_data_api/include/UserDataAPI.hpp>
```

## API Methods
User Data API uses the `user_data` namespace. Its methods are included in `<hiimjasmine00.user_data_api/include/UserDataAPI.hpp>`.

- `user_data::contains(cocos2d::CCNode* node, std::string_view id)`: Checks if user data for the given mod ID (defaults to your mod ID) exists on the node.
- `user_data::get<T>(cocos2d::CCNode* node, std::string_view id)`: Retrieves user data associated with the node and mod ID (defaults to your mod ID), and converts it to type `T` (defaults to [`matjson::Value`](https://docs.geode-sdk.org/classes/matjson/Value/)).
- `user_data::upload(const matjson::Value& data, std::string_view id)`: Uploads user data to the API for the given mod ID (defaults to your mod ID).

## Events
The User Data API exposes [events](https://docs.geode-sdk.org/tutorials/events/) that let you react to web requests (such as loading profiles, comments, or leaderboards).

You can include them all with:
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
```

Or include only the specific event headers you need (see below). Each event also supports an optional account ID filter to limit results.

### Profile Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <hiimjasmine00.user_data_api/include/events/Profile.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::ProfileFilter());
}

// Node-based listener
class $modify(ProfilePage) {
    struct Fields {
        bool m_loaded;
    };

    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        auto fields = m_fields.self();
        if (fields->m_loaded) return;

        fields->m_loaded = true;

        addEventListener<user_data::ProfileFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

### Comment Events
```cpp
#include <Geode/binding/GJComment.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/Comment.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJComment* comment) {
        // Do something
    }, user_data::CommentFilter());
}

// Node-based listener
class $modify(CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        if (m_accountComment) return;

        addEventListener<user_data::CommentFilter>([](GJComment* comment) {
            // Do something
        }, comment->m_accountID);
    }
};
```

### Friend & Blocked User Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/GJUserCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/Friend.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::FriendFilter());
}

// Node-based listener
class $modify(GJUserCell) {
    void loadFromScore(GJUserScore* score) {
        GJUserCell::loadFromScore(score);

        auto friendReqStatus = score->m_friendReqStatus;
        if (friendReqStatus != 1 && friendReqStatus != 2) return;

        addEventListener<user_data::FriendFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

### Friend Request Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/GJRequestCell.hpp>
#include <Geode/modify/GJUserCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/FriendRequest.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::FriendRequestFilter());
}

// Node-based listener
class $modify(GJRequestCell) {
    void loadFromScore(GJUserScore* score) {
        GJRequestCell::loadFromScore(score);

        addEventListener<user_data::FriendRequestFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
class $modify(GJUserCell) {
    void loadFromScore(GJUserScore* score) {
        GJUserCell::loadFromScore(score);

        if (score->m_friendReqStatus != 4) return;

        addEventListener<user_data::FriendRequestFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

### Leaderboard Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/GJScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/GlobalScore.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::GlobalScoreFilter());
}

// Node-based listener
class $modify(GJScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJScoreCell::loadFromScore(score);

        if (score->m_scoreType == 2) return;

        addEventListener<user_data::GlobalScoreFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

### Level Leaderboard Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/GJLevelScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/LevelScore.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::LevelScoreFilter());
}

// Node-based listener
class $modify(GJLevelScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJLevelScoreCell::loadFromScore(score);

        addEventListener<user_data::LevelScoreFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

### User Search Events
```cpp
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/GJScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/events/SearchResult.hpp>

// Global listener
$on_mod(Loaded) {
    new geode::EventListener(+[](GJUserScore* score) {
        // Do something
    }, user_data::SearchResultFilter());
}

// Node-based listener
class $modify(GJScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJScoreCell::loadFromScore(score);

        if (score->m_scoreType != 2) return;

        addEventListener<user_data::SearchResultFilter>([](GJUserScore* score) {
            // Do something
        }, score->m_accountID);
    }
};
```

## License
This mod is licensed under the [MIT License](https://github.com/hiimjasmine00/UserDataAPI/blob/main/LICENSE).
