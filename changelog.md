# User Data API Changelog
## v2.0.0 (2026-02-08)
- Ported to Geometry Dash v2.208 / Geode SDK v5.0.0
- Removed constant references from user_data::upload parameters

## v1.2.4 (2025-10-25)
- Reverted downloading logic changes from v1.2.0 due to bandwidth issues

## v1.2.3 (2025-10-11)
- Changed the server to a new one hosted by [dankmeme01](user:9735891)
- Reduced the amount of data requested from the server when fetching account comments

## v1.2.2 (2025-09-13)
- Fixed a bug where the `user_data::handle...` functions would copy the lambda instead of moving it, causing issues with captures

## v1.2.1 (2025-09-07)
- Fixed hook priority not being set correctly (I blame GitHub Copilot for this one)

## v1.2.0 (2025-09-07)
- Changed downloading logic to download concurrently with vanilla requests
- Added `user_data::handle...` helper functions to simplify event handling in table cells and profile pages
- Added `user_data::ProfileCommentEvent` and `user_data::ProfileCommentFilter` for profile comments

## v1.1.0 (2025-08-28)
- Added `user_data::downloading` method to check if a node is currently downloading user data
- Added `user_data::ID` constant for the mod ID
- Added null checks for node pointers in API methods

## v1.0.0 (2025-08-23)
- Initial release
