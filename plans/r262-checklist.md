# R262 — Social Use Cases: JoinClan, LeaveClan, CreateTeam, DisbandTeam + Full Test Suite

## Objective
Extend the social application layer with four new use cases and provide a
complete Catch2 unit-test suite for all 13 social use cases.

---

## Deliverables

### New Port
- [x] `src/v3/application/ports/include/application/ports/team_repository.hpp`
  — `ITeamRepository` with `find_by_id`, `find_by_member`, `save`, `remove`

### New Use Case Headers
- [x] `src/v3/application/social/include/application/social/join_clan.hpp`
- [x] `src/v3/application/social/include/application/social/leave_clan.hpp`
- [x] `src/v3/application/social/include/application/social/create_team.hpp`
- [x] `src/v3/application/social/include/application/social/disband_team.hpp`

### New Use Case Implementations
- [x] `src/v3/application/social/src/join_clan.cpp`
- [x] `src/v3/application/social/src/leave_clan.cpp`
- [x] `src/v3/application/social/src/create_team.cpp`
- [x] `src/v3/application/social/src/disband_team.cpp`

### Updated CMakeLists
- [x] `src/v3/application/social/CMakeLists.txt` — added 4 new sources

### Test Files (13 total)
- [x] `tests/unit/application/social/add_friend_test.cpp`
- [x] `tests/unit/application/social/remove_friend_test.cpp`
- [x] `tests/unit/application/social/list_friends_test.cpp`
- [x] `tests/unit/application/social/create_clan_test.cpp`
- [x] `tests/unit/application/social/disband_clan_test.cpp`
- [x] `tests/unit/application/social/invite_to_clan_test.cpp`
- [x] `tests/unit/application/social/kick_from_clan_test.cpp`
- [x] `tests/unit/application/social/promote_clan_member_test.cpp`
- [x] `tests/unit/application/social/set_clan_motd_test.cpp`
- [x] `tests/unit/application/social/join_clan_test.cpp`
- [x] `tests/unit/application/social/leave_clan_test.cpp`
- [x] `tests/unit/application/social/create_team_test.cpp`
- [x] `tests/unit/application/social/disband_team_test.cpp`

### Test Infrastructure
- [x] `tests/unit/application/social/CMakeLists.txt` — 13 `pvpgn_v3_add_test` entries
- [x] `tests/unit/application/CMakeLists.txt` — added `add_subdirectory(social)`

---

## Design Notes

### ITeamRepository (new port)
`ILadderRepository` only exposes `get_rank`, `save_entry`, `get_top_n` — no
team CRUD. A dedicated `ITeamRepository` port was created to keep the
ladder and team concerns separate.

### Error types
All social use cases return `core::Result<T, XxxError>` where `XxxError` is a
scoped `enum class : std::uint8_t`. No `core::Status` is used.

### Test strategy
- **Clan tests** use `infra::inmemory::InMemoryClanRepository` +
  `infra::inmemory::InMemoryEventBus` (real in-memory adapters).
- **Friend tests** use `infra::inmemory::InMemoryFriendListRepository` +
  `infra::inmemory::InMemoryAccountRepository`.
- **Team tests** use an inline `FakeTeamRepository` (no `InMemoryTeamRepository`
  exists yet) + `infra::inmemory::InMemoryEventBus`.

### Seeding pattern
`domain::social::Clan::create()` returns `core::Result<Clan>` (value, not
pointer). Use `.value()` to extract before passing to `repo.save()`.
`repo.find_by_id()` returns `core::Result<shared_ptr<Clan>>` — use
`clan_r.value()` (shared_ptr) or `*clan_r.value()` (Clan&) as needed.

### AccountId / TeamId comparison
`core::StrongId` defines `operator<=>` (defaulted), so `operator==` is
available for direct comparison in `REQUIRE` assertions.
