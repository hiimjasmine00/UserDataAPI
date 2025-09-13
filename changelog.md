# User Data API Changelog
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
