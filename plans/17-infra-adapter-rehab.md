# Plan 17 — Infra Adapter Rehabilitation & Build Cleanup

Status: in progress (driven by `docker build -f Dockerfile.v3 --target v3-test`).
Captured while grinding through pre-existing rot exposed by switching from a
curated build-target list to "build everything".

This plan is the running checklist of the **big goals** the cleanup pass has
surfaced. Each section names a concrete deliverable; tactical fixes (single
sign-conversion casts, BOM stripping, etc.) are not tracked here.

---

## G1 — Re-align `infra/sqlite/*` stubs with current application ports

All ten SQLite repositories were written against an earlier port surface and
no longer compile. Each one must be re-declared and re-stubbed (returning
`StatusCode::Unimplemented`) against the current port:

| File | Port | Required change |
| --- | --- | --- |
| `account_ban_repository.{hpp,cpp}` | `IAccountBanRepository` | rename `find/save/remove/forEach/size` → `find_active_ban/add_ban/remove_ban/for_each`; drop `domain::shared::Ban`; use `application::ports::AccountBan` |
| `account_repository.{hpp,cpp}` | `IAccountRepository` | already aligned; only fix `StatusCode::NotImplemented` → `Unimplemented`, ensure `domain::identity::Account` includes are correct |
| `clan_repository.{hpp,cpp}` | `IClanRepository` | `find_by_tag(std::string_view)`, add `find_by_name`, return `Result<shared_ptr<Clan>,Error>`, `remove(std::string_view)`; drop `forEach/size`; drop `domain::ClanTag` |
| `friend_list_repository.{hpp,cpp}` | `IFriendListRepository` | full rewrite to `find_by_owner` + `save` only |
| `ip_ban_repository.{hpp,cpp}` | `IIpBanRepository` | full rewrite to 8-method port (`is_banned`, `add_ban`, `add_range_ban`, `remove_ban`, `remove_range_ban`, `for_each_entry`, `load_banlist`, `save_banlist`); drop `domain::shared::IpBan` |
| `ladder_repository.{hpp,cpp}` | `ILadderRepository` | `get_rank`, `save_entry`, `get_top_n`; use `domain::ladder::LadderEntry`; drop `domain::gameplay::*` |
| `realm_repository.{hpp,cpp}` | `IRealmRepository` | `find_by_id(uint32_t)`, `find_by_name(const std::string&)`, add `remove(uint32_t)`; drop `domain::gameplay::Realm` |
| `sqlite_channel_repository.{hpp,cpp}` | `IChannelRepository` | already aligned; only fix `StatusCode::NotImplemented` |
| `unit_of_work.{hpp,cpp}` | `IUnitOfWork` | already aligned |
| `unit_of_work_factory.{hpp,cpp}` | `IUnitOfWorkFactory` | already aligned |

Acceptance: `pvpgn_infra_sqlite` builds clean against current ports with
every method body returning `Unimplemented`.

---

## G2 — Drop stale `domain::shared::*` types from all infra adapters

Symbols `domain::shared::Ban`, `domain::shared::IpBan`, `domain::ClanTag`,
`domain::gameplay::Realm`, `domain::gameplay::LadderEntry` no longer exist.
Every adapter that still references them must be ported to the surviving
types in `domain::moderation`, `domain::social`, `domain::realm`,
`domain::ladder`, `domain::identity`. Already converted:
`infra/file/ip_ban_repository.{hpp,cpp}`. Remaining: see G1.

---

## G3 — Restore missing `application/ports/*.hpp` headers

Several infra in-memory adapters include port headers that were never
created during the hexagonal refactor. Create minimal interfaces matching
the consumers' overrides:

- ✅ `application/ports/password_hasher.hpp` (done)
- ✅ `application/ports/mail_store.hpp` (done)
- ✅ `application/ports/news_store.hpp` (done; field `text`, not `body`)
- ✅ `application/ports/helpfile_source.hpp` (done; with `all_commands`)
- ✅ `application/ports/icon_provider.hpp` (done)
- ✅ `application/ports/random_source.hpp` (done; `next_uint`, `fill_bytes`)
- ⏳ `application/ports/session_token_issuer.hpp` (consumer: `infra/inmemory/in_memory_session_token_issuer.hpp`)

Acceptance: every header under `src/infra/inmemory/include/...` resolves.

---

## G4 — Plug missing Alpine packages into `Dockerfile.v3`

Build-time failures from missing system libraries:

- ✅ `sqlite-dev` (sqlite3.h) — added.
- ✅ `spdlog-dev` (spdlog headers) — added.
- ⏳ Audit remaining `fatal error: … No such file` lines after next round.

Acceptance: no `fatal error:` lines for system headers in the build log.

---

## G5 — Forward-declare `core::IMetricsRegistry` without re-declaring

`application::ports::IMetricsRegistry` is a `using` alias for
`core::IMetricsRegistry`. Headers that previously forward-declared
`class application::ports::IMetricsRegistry;` now conflict. Fix sites by
forward-declaring `core::IMetricsRegistry` and re-aliasing in
`application::ports`. Done for `src/infra/net/include/infra/net/signal_handler.hpp`.

Acceptance: `grep "class IMetricsRegistry;" src/**/include` returns no
sites under `application::ports::`.

---

## G6 — Replace `unordered_map::operator[]` with `insert_or_assign` for non-default-constructible values

`domain::realm::Realm` and `domain::realm::Character` have no default ctor,
so `map[key] = value;` is ill-formed under libstdc++ 15. Already fixed in:
- `src/infra/persistence/realm/src/inmemory_character_repository.cpp`
- `tests/unit/application/realm/{heartbeat_realm,realm_auth,register_realm,unregister_realm,join_game_server,load_character,save_character}_test.cpp`

Acceptance: no remaining `_[key] = realm|character|c;` patterns under
`tests/unit/application/realm/`.

---

## G7 — `protocol/bnet/messages/*.hpp` include-path normalisation

Headers under `src/protocol/bnet/include/protocol/bnet/messages/*.hpp` were
including `"messages/messages_common.hpp"` (relative to the parent dir)
instead of `"messages_common.hpp"` (sibling). All siblings fixed and any
UTF-8 BOMs introduced by the rewrite stripped. Audit: keep an eye out for
similar patterns in `protocol/d2cs/`, `protocol/d2dbs/`, `protocol/wol/`.

---

## G8 — gcc 15 + libstdc++ 15 false-positives

Globally demote these to warnings in `cmake/v3_warnings.cmake`:

- ✅ `-Wno-error=free-nonheap-object` (false positive on
  `std::vector<std::byte>::push_back` after `reserve`).
- ⏳ Consider `-Wno-error=null-dereference` for tests that read files with
  `std::istreambuf_iterator` — currently worked around by switching to
  `std::ostringstream << rdbuf()`.

---

## G9 — Sol2 / Lua plumbing

Already landed:
- `sol2` marked `SYSTEM PRIVATE` with `-Wno-error=template-body`.
- `lua[key]` accesses rewritten to `lua[key].get<T>()` in
  `sol2_script_host.cpp` and `lua_api_v2.cpp`.

Remaining risk: any new sol2 site must keep this pattern; consider a
short paragraph in `15-large-file-decomposition-detail.md` or
`04-lua-to-scripts-migration.md`.

---

## G10 — UnregisterRealm test depends on RegisterRealm

`tests/unit/application/realm/unregister_realm_test.cpp` references
`pa::RegisterRealm` and `pa::RegisterRealmCommand` without including
`application/realm/register_realm.hpp`. Fixed; capture as a reminder that
the use-case headers are not transitively exposed by the unregister header.

---

## G11 — `Secret<std::string>` no longer compares against `const char*`

All test sites that previously wrote
`REQUIRE(cfg.storage.dsn == "file:pvpgn.db")` must use
`.reveal() == "file:pvpgn.db"` instead. Fixed in
`tests/unit/infra/config/server_config_test.cpp`,
`tests/unit/infra/config/legacy_prefs_test.cpp`,
`tests/functional/config_roundtrip_test.cpp`. Audit any new test code.

---

## Acceptance for Plan 17

1. `docker build -f Dockerfile.v3 --target v3-test` succeeds.
2. `ctest --test-dir build/v3 -C Release` runs all v3 tests (failures
   permitted at this stage; this plan tracks compile/link cleanliness).
3. No `pvpgn_v3_*`-prefixed link names in any `CMakeLists.txt`.
4. No `HEADERS` keyword passed to `pvpgn_v3_add_library`.
5. No re-declarations of `class IMetricsRegistry` inside
   `application::ports`.
6. Every infra adapter in `src/infra/{sqlite,file,inmemory}/...` matches
   its `application/ports` interface exactly.

---

## Cross-plan references

- Plan 09 (build-system modernization) — `pvpgn_v3_add_library` ergonomics.
- Plan 07 (bounded contexts & layering) — `application` ↛ `infra` rule
  enforced by `cmake/layering_exceptions.txt`.
- Plan 05 / 15 (large-file decomposition) — protocol/bnet/messages split
  surfaced G7.
- Plan 14 (Lua → plugins migration) — G9 sol2 hardening unblocks this.
