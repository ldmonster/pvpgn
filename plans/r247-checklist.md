# R247 — bnetd small-module lifecycle bridges (third batch)

Status: ✅ GREEN — 12 cases / 47 assertions.

## Scope

Strangler-fig observation bridges over four more bnetd legacy
modules.

| Module             | Symbol(s)                                                                                                              | Level |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------- | ----- |
| alias_command      | `_aliasfile_load_try(filename)`, `_aliasfile_unload_try`, `_handle_alias_command_try(sd, text)`                        | Info/Debug |
| command_groups     | `_command_groups_load_try(filename)`, `_command_groups_unload_try`, `_command_groups_reload_try(filename)`             | Info |
| anongame_maplists  | `_anongame_maplists_create_try`, `_anongame_maplists_destroy_try`, `_anongame_tournament_maplists_destroy_try`         | Info |
| handle_udp         | `_handle_udp_packet_try(usock, src_addr, src_port)`                                                                    | Trace |

Per-function module strings `v3_bnetd_<mod>_bridge` so log filters
still see individual subsystems.

## Files added

- src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/bnetd_lifecycle_bridges_r247.hpp
- src/v3/integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r247.cpp
- tests/unit/integration/legacy_bnetd/bnetd_lifecycle_bridges_r247_test.cpp
- plans/r247-checklist.md

## Files modified (legacy wiring)

- src/bnetd/alias_command.cpp — guarded calls in `aliasfile_load`,
  `aliasfile_unload`, `handle_alias_command` (sd via
  `conn_get_socket(c)`, -1 sentinel for null).
- src/bnetd/command_groups.cpp — guarded calls in
  `command_groups_load`, `command_groups_unload`,
  `command_groups_reload`.
- src/bnetd/anongame_maplists.cpp — guarded calls in
  `anongame_maplists_create`, `anongame_maplists_destroy`,
  `anongame_tournament_maplists_destroy`.
- src/bnetd/handle_udp.cpp — guarded call in `handle_udp_packet`
  (`src_port` widened to `unsigned int`).

All wires are gated by `PVPGN_V3_BNETD_INTEGRATION`; legacy paths
fall through unchanged.

## Files modified (build / CI)

- src/v3/CMakeLists.txt — added
  `integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r247.cpp`
  to `integration_legacy_bnetd` SOURCES.
- tests/unit/integration/legacy_bnetd/CMakeLists.txt — registered
  `test_integration_legacy_bnetd_lifecycle_bridges_r247`.
- Dockerfile.v3 — added target to both the v3 build target list
  and the v3-test RUN chain.

## Verification

```
$ cmake --build build -j8 --target \
    integration_legacy_bnetd \
    test_integration_legacy_bnetd_lifecycle_bridges_r247 \
    bnetd_legacy bnetd
$ ./build/tests/unit/integration/legacy_bnetd/\
test_integration_legacy_bnetd_lifecycle_bridges_r247 --reporter compact
All tests passed (47 assertions in 12 test cases)
```

`bnetd` and `bnetd_legacy` link cleanly with the new guarded wires.

## Lessons applied

- For multi-field `bridge_log_kv` calls the `std::span` overload
  must be constructed via the initializer-list form
  `{fields[0], fields[1], ...}` (not the `{ptr, count}` pair —
  that doesn't match `std::initializer_list<Field>`).
- Sub-int legacy scalars (`unsigned short src_port`) are widened
  at the ABI boundary to `unsigned int`.
- `conn_get_socket(c)` + `-1` sentinel for pointer-to-int in the
  user-driven `handle_alias_command` path.
