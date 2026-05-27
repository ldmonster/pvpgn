# R196.f -- Fix prefs_v3::effective_user/effective_group to return nullptr for missing toml keys

## Goal

Eliminate the paper-cut identified during R196.e:
`pvpgn_v3_prefs_get_effective_user()` returned `""` (empty C string)
when the toml field was commented out or set to `""`. The downstream
`give_up_root_privileges(user_name, group_name)` in
`src/common/give_up_root_privileges.cpp` only treats `nullptr` as
"skip the privilege change"; any non-null pointer (including the empty
string) is fed to `getpwnam()`, which returns NULL for the empty case,
which `gurp_uname2id` maps to `ILLEGAL_ID`, which aborts startup with
`main: could not give up privileges`. The legacy non-v3 path
(`prefs_runtime_config.effective_user`) leaves the field as NULL when
unset via `conf_setdef_effective_user` -> `conf_set_str(..., NULL, NULL)`,
so the v3 bridge was inconsistent with the legacy behaviour.

## Why this matters

The R196.e smoke script worked around the bug by `sed`-injecting
`effective_user = "root"` into `bnetd.toml` before launching the
daemon. With the proper fix, that workaround is no longer needed --
the daemon honours the original toml semantics ("commented out =
don't change uid") and the shipped `bnetd.toml.in` works as-is.

## Implementation

### 1. `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp`

**Before** (returns `""` when prefs not loaded; returns `.data()` of
the backing `std::string` which is `""` when the toml key is missing):

```cpp
extern "C" const char* pvpgn_v3_prefs_get_effective_user() noexcept {
    auto p = g_prefs.load(); return p ? p->effective_user().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_effective_group() noexcept {
    auto p = g_prefs.load(); return p ? p->effective_group().data() : "";
}
```

**After** (returns `nullptr` in both the not-loaded case and the
empty-string case; only returns the C string when there is real
content):

```cpp
// Note: returns nullptr (NOT empty string) when the toml field is missing
// or set to "". give_up_root_privileges() in src/common treats nullptr as
// "skip privilege change" and treats any non-null pointer as "try to
// setuid/setgid to this name" -- so returning "" would make getpwnam("")
// fail and abort startup. The legacy prefs path (prefs_runtime_config.*)
// also returns NULL for the unset case, so this matches that semantic.
// string_view::data() is null-terminated here because the backing store
// is a std::string (legacy_prefs.hpp: effective_user_str_).
extern "C" const char* pvpgn_v3_prefs_get_effective_user() noexcept {
    auto p = g_prefs.load();
    if (!p) return nullptr;
    auto sv = p->effective_user();
    return sv.empty() ? nullptr : sv.data();
}
extern "C" const char* pvpgn_v3_prefs_get_effective_group() noexcept {
    auto p = g_prefs.load();
    if (!p) return nullptr;
    auto sv = p->effective_group();
    return sv.empty() ? nullptr : sv.data();
}
```

### 2. `scripts/dev/v3-smoke-runtime.sh` -- remove workaround

Removed the `sed -e 's/^[[:space:]]*#[[:space:]]*effective_user/.../'`
block + replaced its comment with a pointer to R196.f explaining why
no injection is needed.

## Verification

Rebuilt `v3-smoke` stage *without* the workaround. Smoke result:

```
==> starting bnetd --debug -c /tmp/pvpgn-conf/bnetd.toml
PASS: bnetd alive after 6s (pid 57)
--- listen ports ---
State  Recv-Q Send-Q Local Address:Port Peer Address:Port
LISTEN 0      10           0.0.0.0:6112      0.0.0.0:*
PASS: bnetd listening on :6112
==> bnetd smoke complete
==> starting d2cs --debug -c /tmp/pvpgn-conf/d2cs.toml
PASS: d2cs alive after 6s (pid 66)
--- listen ports ---
State  Recv-Q Send-Q Local Address:Port Peer Address:Port
LISTEN 0      10           0.0.0.0:6113      0.0.0.0:*
PASS: d2cs listening on :6113
==> d2cs smoke complete
naming to docker.io/library/pvpgn-v3-smoke:r196f done
```

The fact that **bnetd starts cleanly without `effective_user = "root"`
in the toml** is the actual test of R196.f: the only path that lets the
daemon proceed past `give_up_root_privileges` is for the accessor to
return `nullptr`, which exercises the "skip privilege change" branch.

## Why this is correct beyond just "the smoke passes"

1. **Matches legacy parity**: `prefs_runtime_config.effective_user`
   is initialised to NULL via `conf_setdef_effective_user` ->
   `conf_set_str(..., NULL, NULL)`. The v3 bridge now matches that
   "unset means NULL" contract.
2. **Matches the consumer API contract**:
   `give_up_root_privileges(char const* user_name, char const* group_name)`
   in `src/common/give_up_root_privileges.cpp` documents (via its
   `if (user_name)` / `if (group_name)` guards) that nullptr means
   "skip". Returning `""` violated that contract.
3. **Matches toml-file semantics**: in the .toml file, having the line
   commented out and having it set to `""` are equivalent ways of
   expressing "no override" -- both should map to "skip". Previously
   they both errored out fatally, which makes the shipped commented-out
   template unusable for any non-root deployment that doesn't manually
   uncomment it.

## Files changed

| File | Change |
|------|--------|
| `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp` | +18 / -4 lines |
| `scripts/dev/v3-smoke-runtime.sh` | +6 / -10 lines (workaround removed) |

## Out of scope / deferred

- Audit of every other `pvpgn_v3_prefs_get_*()` accessor for the same
  empty-string-instead-of-nullptr pattern. There are ~100 of them.
  Only `effective_user` / `effective_group` are known to have callers
  that distinguish nullptr from "" so far. Other string-returning
  accessors (e.g. log paths, motd) tend to be handed straight to
  `fopen` etc., which both fail equivalently on "" and on nullptr.
- `d2cs_prefs_bridge.cpp`: not affected -- d2cs has no effective_user
  / effective_group fields.

## Status

- `pvpgn-v3-smoke:r196f` image GREEN (built end-to-end including the
  removal of the workaround).
- R196.e checklist is unchanged but its "Out of scope / deferred"
  section's prefs_v3 followup item is now closed by this round.
