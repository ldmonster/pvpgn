# R271 — Expand InMemory Adapter Test Suite

## Status: COMPLETE

## Test Files Created

- `tests/unit/infra/inmemory/in_memory_team_repository_test.cpp`
- `tests/unit/infra/inmemory/in_memory_mail_store_test.cpp`
- `tests/unit/infra/inmemory/in_memory_news_store_test.cpp`
- `tests/unit/infra/inmemory/in_memory_helpfile_source_test.cpp`
- `tests/unit/infra/inmemory/in_memory_icon_provider_test.cpp`
- `tests/unit/infra/inmemory/in_memory_random_source_test.cpp`
- `tests/unit/infra/inmemory/in_memory_session_token_issuer_test.cpp`
- `tests/unit/infra/inmemory/in_memory_message_broadcaster_test.cpp`
- `tests/unit/infra/inmemory/in_memory_permission_checker_test.cpp`
- `tests/unit/infra/inmemory/in_memory_config_subscriber_test.cpp`
- `tests/unit/infra/inmemory/in_memory_channel_store_test.cpp`

## Files Modified

- `tests/unit/infra/inmemory/CMakeLists.txt` — added all 11 new test sources to
  `test_infra_inmemory`; added a separate `test_infra_inmemory_config_subscriber`
  target guarded by `if(TARGET infra_config)` because `InMemoryConfigSubscriber`
  depends on `infra/config/server_config.hpp` which is only available when
  `PVPGN_V3_WITH_TOMLPP=ON`.

## Test Coverage Summary

| Fake | Tests |
|------|-------|
| `InMemoryTeamRepository` | `find_by_id` not-found, `find_by_member` empty, `remove` not-found (domain `Team` construction skipped — no public factory) |
| `InMemoryMailStore` | send/inbox, multi-message, per-recipient isolation, delete by index, not-found errors |
| `InMemoryNewsStore` | empty store, add/get, newest-first ordering, max_items cap, zero max |
| `InMemoryHelpfileSource` | lookup miss, set/lookup, overwrite, `all_commands`, case-sensitivity |
| `InMemoryIconProvider` | icon miss, set/lookup, overwrite, multi-icon isolation, raw data empty/set/overwrite |
| `InMemoryRandomSource` | range bounds, min==max, deterministic seed, different seeds, `fill_bytes` size/non-zero/empty span, default ctor |
| `InMemorySessionTokenIssuer` | non-empty token, unique tokens, validate issued, validate unknown, revoke, revoke no-op, revoke isolation, multi-token per account |
| `InMemoryMessageBroadcaster` | empty initially, broadcast/send_info/send_error capture, accumulate, clear, clear-then-add, channel-id in message |
| `InMemoryPermissionChecker` | default false, grant/check, revoke, revoke no-op, multi-perm, per-account isolation, groups (grant/check/revoke/no-op/multi/isolation), duplicate grant idempotent |
| `InMemoryConfigSubscriber` | count zero, notify increments, `on_config_reloaded`, callback invoked, multi-callbacks, correct config passed, multi-notify count |
| `InMemoryChannelStore` | load_all empty, save/load, multi-save, overwrite by name, remove, remove not-found, remove isolation, field preservation |

## Skipped

- Full save/find round-trip for `InMemoryTeamRepository` — `domain::social::Team`
  has no public default constructor or factory accessible from tests; the
  not-found and empty-collection paths are covered instead (same pattern as the
  existing `InMemoryClanRepository` test).
