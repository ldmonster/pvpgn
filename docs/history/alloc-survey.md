# pvpgn x-alloc survey

This document catalogues the remaining `xmalloc`/`xrealloc`/`xfree`/
`xstrdup` (and bare `std::malloc`/`std::free`) call sites in
`src/`, grouped by the modernization pattern that applies.
The goal is to phase them out in favor of standard library
containers (`std::string`, `std::vector<T>`,
`std::unique_ptr<T>`) and RAII allocation (`new T{}` / `delete`).

## Status

- Total raw matches across `src/**/*.cpp` exceed 500 (the workspace
  grep capped at 200).
- A single sweeping conversion is not tractable in one round; the
  per-field / per-function approach used for `t_connection` (see
  `plans/step4-checklist.md`) is the only safe path.

## Categories (in priority order)

### 1. Transient local buffers built from `strlen(...) + 1` (LOW RISK)

These are local-scope `char *foo = (char*)xmalloc(strlen(a)+strlen(b)+...);
sprintf(foo, "%s/%s", a, b); ...; xfree(foo);` patterns. They have no
escapees and can be replaced by `std::string` with `fmt::format`,
`+=`, or `std::filesystem::path` directly.

Known sites:
- `src/d2cs/d2charfile.cpp` lines 232, 241, 288, 339, 371, 430, 440,
  448, 458, 504, 561 -- 11 nearly identical
  `path = xmalloc(strlen(prefs_get_X_dir())+1+...)` blocks.
- `src/d2cs/d2ladder.cpp` line 69 -- ladderfile path.
- `src/d2cs/handle_d2cs.cpp` lines 176, 844, 956 -- charinfo path.
- `src/bnetd/anongame_wol.cpp` line 541 -- DONE in this round
  (replaced with `std::vector<char>`).

### 2. `xstrdup` for short-lived working copies (LOW RISK)

Caller takes a `char const *` argument, makes a writable duplicate,
strtok / strchr / etc., then frees. Replace with
`std::string temp(src); ...std::strtok(temp.data(), ",");`.

Done this round:
- `src/d2cs/handle_signal.cpp` -- DONE.
- `src/d2cs/main.cpp` (loglevels block) -- DONE.
- `src/d2cs/s2s.cpp` -- DONE.

Remaining:
- `src/d2cs/main.cpp` (`write_to_pidfile` returns `xstrdup`'d buffer --
  requires changing the function signature plus all callers; medium
  risk).
- `src/bnetd/alias_command.cpp` lines 106, 393, 413, 467 -- alias /
  output line buffers, freed in the same scope.
- `src/bnetd/attrgroup.cpp` lines 375, 383 -- key dup for hash
  insertion; both stored into `t_hashtable` and freed in the destroy
  callback. These leak ownership into a container -- defer until
  attrgroup itself is migrated to `std::unordered_map<std::string,
  ...>`.

### 3. Struct allocations with simple lifetimes (MEDIUM RISK)

`foo = (t_foo *)xmalloc(sizeof(t_foo)); ...; xfree(foo);` pairs. The
mechanical conversion is `foo = new t_foo{}; ...; delete foo;`, but
some structs carry `xmalloc`'d / `xstrdup`'d members which need to be
freed by the destroy function -- those should be moved to
`std::string` / `std::vector` first (the `t_connection` round 1
playbook).

Known sites:
- `src/bnetd/anongame.cpp` lines 534, 945, 1351 -- t_matchdata,
  t_saf_pt2, t_anongameinfo.
- `src/bnetd/anongame_gameresult.cpp` lines 75, 77, 111 --
  t_anongame_gameresult plus its `players[]` and `heroes[]` arrays
  (the arrays are obvious `std::vector<...>` candidates).
- `src/bnetd/anongame_wol.cpp` line 92 -- t_anongame_wol_player.
- `src/bnetd/attrgroup.cpp` line 118 -- t_attrgroup (depends on
  category 2 first).
- `src/bnetd/anongame_infos.cpp` lines 60, 99, 102, 195, 251, 272,
  337, 420, 1945, 1950, 1979, 1998 -- complex; many xmalloc'd
  member arrays.
- `src/d2cs/connection.cpp` line 390 -- d2cs t_connection (mirrors the
  bnetd work but in the d2cs namespace).
- `src/d2cs/d2gs.cpp` line 157 -- t_d2gs.
- `src/d2cs/d2charlist.cpp` line 44 -- t_d2charlist.
- `src/d2cs/d2ladder.cpp` lines 90, 106, 126, 143 -- t_d2ladder and
  its info[] arrays.
- `src/d2cs/handle_d2cs.cpp` lines 862, 980 -- t_d2charinfo_file.
- `src/d2cs/serverqueue.cpp` line 104 -- t_sq.

### 4. Per-element arrays inside larger structures (MEDIUM RISK)

`obj->items = (t_item *)xmalloc(sizeof(t_item) * n);` -- these are
prime `std::vector<t_item>` candidates but require touching every
caller that does indexed access.

Known sites:
- `src/bnetd/anongame_maplists.cpp` lines 110, 132, 154, 176 --
  maplist_war3 / maplist_w3xp / maplist_ral2 / maplist_yuri (4 global
  arrays of map names, all `xstrdup`'d).
- `src/bnetd/account_wrap.cpp` lines 165, 190 -- transient `char *`
  for base64 / hex string formatting.

### 5. Globals / singletons with manual init+destroy (LOWER PRIORITY)

`anongame_infos_URL`, `anongame_infos_DESC` etc. in
`src/bnetd/anongame_infos.cpp` are file-scope `xmalloc`'d arrays.
They live for the lifetime of the daemon and are freed at shutdown.
Mechanical risk is low but rewards are minimal -- defer until the
surrounding feature is rewritten in a more substantial way.

## Plan suggestion

1. Continue per-field `t_connection` migration (rounds 2-N) until the
   bnetd connection has no `char const *` members.
2. Apply the same per-field treatment to `d2cs::t_connection` (one
   round).
3. Sweep category 1 (transient path strings) in `d2cs/d2charfile.cpp`
   in one go -- the 11 sites are copy-paste identical and benefit
   most from `std::filesystem::path`.
4. Tackle `anongame_gameresult` (category 3) -- small isolated struct
   with two array members -- as a self-contained `std::vector`
   conversion.

## Done this round

- `src/d2cs/handle_signal.cpp`: loglevels xstrdup/xfree -> std::string.
- `src/d2cs/main.cpp`: loglevels xstrdup/xfree -> std::string.
- `src/d2cs/s2s.cpp`: tserver xstrdup/xfree -> std::string.
- `src/bnetd/anongame_wol.cpp` tokenize_line: line xmalloc/xfree ->
  std::vector<char>.
- `src/bnetd/connection.cpp` t_connection: xmalloc/xfree -> new/delete;
  3 fields (clientexe, clientver, loggeduser) char* -> std::string.
