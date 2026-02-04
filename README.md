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
            "version": ">=v1.2.0",
            "required": true
        }
    }
}
```

`"required"` can be set to `false` if you want your mod to work without User Data API.

Then, include the headers in your code.

**Required dependency:**
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
#include <hiimjasmine00.user_data_api/include/UserDataAPI.hpp>
```

**Non-required dependency:**
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
#define USER_DATA_API_EVENTS
#include <hiimjasmine00.user_data_api/include/UserDataAPI.hpp>
```

## API Methods
User Data API uses the `user_data` namespace. Its methods are included in `<hiimjasmine00.user_data_api/include/UserDataAPI.hpp>`.

- `user_data::contains(cocos2d::CCNode* node, std::string_view id)`: Checks if user data for the given mod ID (defaults to your mod ID) exists on the node.
- `user_data::downloading(cocos2d::CCNode* node)`: Checks if user data is currently being downloaded for the node.
- `user_data::get<T>(cocos2d::CCNode* node, std::string_view id)`: Retrieves user data associated with the node and mod ID (defaults to your mod ID), and converts it to type `T` (defaults to [`matjson::Value`](https://docs.geode-sdk.org/classes/matjson/Value/)).
- `user_data::upload(const matjson::Value& data, std::string_view id)`: Uploads user data to the API for the given mod ID (defaults to your mod ID).

The ID for User Data API can be retrieved using `user_data::ID`.

## Events
The User Data API exposes [events](https://docs.geode-sdk.org/tutorials/events/) that let you react to web requests (such as loading profiles, comments, or leaderboards).

You can include them all with:
```cpp
#include <hiimjasmine00.user_data_api/include/Events.hpp>
```

Or include only the specific event headers you need.
- `<hiimjasmine00.user_data_api/include/events/Profile.hpp>`: Profile events
- `<hiimjasmine00.user_data_api/include/events/Comment.hpp>`: Level/list comment events
- `<hiimjasmine00.user_data_api/include/events/ProfileComment.hpp>`: Profile comment events
- `<hiimjasmine00.user_data_api/include/events/Friend.hpp>`: Friend & blocked user events
- `<hiimjasmine00.user_data_api/include/events/FriendRequest.hpp>`: Friend request events
- `<hiimjasmine00.user_data_api/include/events/GlobalScore.hpp>`: Leaderboard events
- `<hiimjasmine00.user_data_api/include/events/LevelScore.hpp>`: Level leaderboard events
- `<hiimjasmine00.user_data_api/include/events/SearchResult.hpp>`: User search events

Each event also supports an optional account ID filter to limit results.

The Events.hpp file also includes helper functions to make adding listeners easier, which are seen below. These functions automatically check if user data is being downloaded, and only call your function when the data is ready.

### Profile Events
```cpp
#include <Geode/modify/ProfilePage.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::ProfileEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        user_data::handleProfilePage(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

### Level/List Comment Events
```cpp
#include <Geode/modify/CommentCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::CommentEvent().listen([](GJComment* comment) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        if (m_accountComment) return;

        user_data::handleCommentCell(this, [this](GJComment* comment) {
            // Do something
        });
    }
};
```

### Profile Comment Events
```cpp
#include <Geode/modify/CommentCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::ProfileCommentEvent().listen([](GJComment* comment) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        if (!m_accountComment) return;

        user_data::handleCommentCell(this, [this](GJComment* comment) {
            // Do something
        });
    }
};
```

### Friend & Blocked User Events
```cpp
#include <Geode/modify/GJUserCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::FriendEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(GJUserCell) {
    void loadFromScore(GJUserScore* score) {
        GJUserCell::loadFromScore(score);

        auto friendReqStatus = score->m_friendReqStatus;
        if (friendReqStatus != 1 && friendReqStatus != 2) return;

        user_data::handleUserCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

### Friend Request Events
```cpp
#include <Geode/modify/GJRequestCell.hpp>
#include <Geode/modify/GJUserCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::FriendRequestEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(GJRequestCell) {
    void loadFromScore(GJUserScore* score) {
        GJRequestCell::loadFromScore(score);

        user_data::handleRequestCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
class $modify(GJUserCell) {
    void loadFromScore(GJUserScore* score) {
        GJUserCell::loadFromScore(score);

        if (score->m_friendReqStatus != 4) return;

        user_data::handleUserCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

### Leaderboard Events
```cpp
#include <Geode/modify/GJScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::GlobalScoreEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(GJScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJScoreCell::loadFromScore(score);

        if (score->m_scoreType == 2) return;

        user_data::handleScoreCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

### Level Leaderboard Events
```cpp
#include <Geode/modify/GJLevelScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::LevelScoreEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(GJLevelScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJLevelScoreCell::loadFromScore(score);

        user_data::handleLevelScoreCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

### User Search Events
```cpp
#include <Geode/modify/GJScoreCell.hpp>
#include <hiimjasmine00.user_data_api/include/Events.hpp>

// Global listener
$on_mod(Loaded) {
    user_data::SearchResultEvent().listen([](GJUserScore* score) {
        // Do something
    }).leak();
}

// Node-based listener
class $modify(GJScoreCell) {
    void loadFromScore(GJUserScore* score) {
        GJScoreCell::loadFromScore(score);

        if (score->m_scoreType != 2) return;

        user_data::handleScoreCell(this, [this](GJUserScore* score) {
            // Do something
        });
    }
};
```

## Credits
- [dankmeme01](https://gdbrowser.com/u/9735891) - Host for the mod's servers
- [hiimjasmine00](https://gdbrowser.com/u/7466002) - Creator of the mod

## License
This mod is licensed under the [MIT License](https://github.com/hiimjasmine00/UserDataAPI/blob/main/LICENSE).
