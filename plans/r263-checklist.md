# R263 — realm: RegisterRealm + UnregisterRealm + HeartbeatRealm + RealmAuth

## Status: ✅ COMPLETE

### Use cases
- [x] register_realm.hpp + register_realm.cpp
- [x] unregister_realm.hpp + unregister_realm.cpp
- [x] heartbeat_realm.hpp + heartbeat_realm.cpp
- [x] realm_auth.hpp + realm_auth.cpp

### Tests
- [x] register_realm_test.cpp — 4 test cases (happy path, duplicate name → Conflict, empty name → InvalidArgument, empty host → InvalidArgument)
- [x] heartbeat_realm_test.cpp — 2 test cases (happy path re-saves realm, unknown realm → NotFound)
- [x] realm_auth_test.cpp — 3 test cases (correct password → realm ID returned, unknown realm → NotFound, wrong password → PermissionDenied)

### Build
- [x] realm/CMakeLists.txt updated (4 new sources + 4 new headers)
- [x] tests/unit/application/realm/CMakeLists.txt updated (3 new test targets)

### Design notes
- All use cases inject `ports::IRealmRepository&` via constructor (no global state).
- `domain::realm::Realm` has no `password_hash` or `last_seen` fields; `RealmAuth`
  introduces an inline `IRealmCredentialStore` port (same pattern as
  `ICharacterRepository` in `character_lock.hpp`) to keep credentials separate
  from the domain aggregate.
- `HeartbeatRealm` re-saves the realm via `IRealmRepository::save()` so that
  infrastructure adapters can update their own `last_seen` / heartbeat column.
- Return type is `core::Result<T, core::Error>` (two-param form) matching the
  existing use-case style in `join_game_server.hpp` / `create_character.hpp`.
- `[[nodiscard]]` applied to all `execute()` methods.
