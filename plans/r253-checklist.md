# R253 — Missing domain value objects

## Checklist

- [x] Added `ConnectionId` to `domain/shared/ids.hpp`
- [x] Created `domain/chat/include/domain/chat/channel_name.hpp`
- [x] Created `domain/realm/include/domain/realm/realm_name.hpp`
- [x] Created `domain/social/include/domain/social/clan_tag.hpp`
- [x] Created `domain/gameplay/include/domain/gameplay/game_name.hpp`
- [x] All 4 new headers are self-contained (no infra/ includes)
- [x] All 4 new headers use only C++20 stdlib
- [x] Unit tests created for all 5 new types
- [x] Tests registered in CMakeLists.txt
- [x] Header selfcheck targets added

## Exit Criterion

`domain::chat::ChannelName::parse("Diablo II")` returns a non-empty optional.
`domain::social::ClanTag::parse("A")` returns nullopt (too short).
`domain::ConnectionId{42}.value() == 42u`.

## Status: GREEN
