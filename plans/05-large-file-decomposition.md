# 05 — Decompose large files

## What

Break files that exceed the hard cap in plan 01 into focused units. Inventory below is the ground truth at the time of writing.

## Inventory (top offenders)

| LOC  | File | Action |
|------|------|--------|
| 8771 | `src/common/pugixml.cpp` | Vendored; move to `vendor/pugixml/` (plan 02). No code change. |
| 6148 | `src/integration/legacy_bnetd/src/handle_bnet_link.cpp` | Split — see 05.1 |
| 4001 | `src/protocol/bnet/src/codec.cpp` | Split — see 05.3 |
| 3649 | `src/common/bnet_protocol.h` | Split — see 05.4 |
| 2499 | `tests/unit/protocol/bnet/codec_test.cpp` | Split — see 05.5 |
| 2342 | `src/integration/legacy_bnetd/src/irc_link.cpp` | Split — see 05.1 |
| 1987 | `conf/versioncheck.json.in` | Reclassify as data (plan 03). No split. |
| 1703 | `src/protocol/bnet/include/protocol/bnet/messages.h` | Split per message family. |
| 1619 | `src/integration/legacy_bnetd/src/handle_wol_link.cpp` | Split — see 05.1 |
| 1303 | `src/common/packet.cpp` | Replace by `infra/net/packet.cpp` (already exists in protocol layer); delete after callers move. |
| 1259 | `tests/unit/domain/connection/connection_fsm_test.cpp` | Split per transition family. |
| 1165 | `tests/unit/protocol/d2cs/fsm_test.cpp` | Split per phase. |
| 1009 | `src/integration/legacy_bnetd/src/handle_anongame_link.cpp` | Split — see 05.1 |
| 857  | `src/protocol/bnet/src/fsm.cpp` | Split states into their own files. |
| 844  | `src/application/connection/src/connection_fsm.cpp` | Split states. |
| 749  | `src/integration/legacy_bnetd/src/handle_apireg_link.cpp` | Split. |
| 749  | `src/protocol/d2cs/src/fsm.cpp` | Split. |
| 712  | `src/tools/bntrackd/bntrackd.cpp` | Extract `tracker_server.{hpp,cpp}` + thin `main.cpp`. |
| 678  | `src/app/bnetd/src/main.cpp` | Extract `bootstrap.{hpp,cpp}` and `signal_handlers.{hpp,cpp}`. |
| 647  | `src/protocol/bnet/src/anongame.cpp` | Split. |
| 540  | `src/infra/config/src/server_config.cpp` | Split per TOML section. |
| 522  | `src/integration/legacy_bnetd/src/prefs_bridge.cpp` | Generate from a `prefs.def` X-macro list; trim by ~70%. |
| 484  | `src/application/connection/include/application/connection/...` | Split. |
| 443  | `conf/autoupdate.conf.in` | Reclassify (plan 03). |
| 364  | `src/infra/config/include/infra/config/legacy_prefs.hpp` | Split into `legacy_prefs/{files,log,network,...}.hpp`. |

## 05.1 — `handle_*_link.cpp` decomposition (legacy bnetd bridges)

These files are giant `switch` statements dispatching incoming packets. Pattern:

```
src/integration/legacy_bnetd/src/handle_bnet_link.cpp        # dispatcher only
src/integration/legacy_bnetd/src/handle_bnet/
  auth.cpp        # PACKET_LOGON_*, PACKET_AUTH_*
  realm.cpp       # PACKET_REALM_*
  chat.cpp        # PACKET_CHAT_*
  game.cpp        # PACKET_GAME_*
  friends.cpp     # PACKET_FRIENDS_*
  clan.cpp        # PACKET_CLAN_*
  ladder.cpp      # PACKET_LADDER_*
  file.cpp        # PACKET_FILE_*
  ads.cpp         # PACKET_AD_*
  misc.cpp        # everything else
```

Each file exposes `int handle_<family>(connection*, packet*)` and the dispatcher in `handle_bnet_link.cpp` is reduced to a flat table:

```cpp
constexpr std::array kHandlers = {
  Entry{ CLIENT_AUTH_INFO,  handle_bnet::auth_info },
  Entry{ CLIENT_AUTH_CHECK, handle_bnet::auth_check },
  ...
};
```

Same shape for `irc_link.cpp`, `handle_wol_link.cpp`, `handle_anongame_link.cpp`, `handle_apireg_link.cpp`.

## 05.2 — `xalloc` removal

The repo already has `docs/migration-xalloc-to-stl.md`. Final passes:

1. Sweep with `scripts/xalloc_sweep.ps1` and replace remaining `xmalloc`/`xfree`/`xstrdup` with `std::vector<std::byte>` / `std::string` / `std::unique_ptr<T[]>`.
2. Delete `src/common/{xalloc,xstr,scoped_array,scoped_ptr,asnprintf}.{cpp,h}`.
3. Remove `--with-xalloc` style configure checks from `ConfigureChecks.cmake`.

## 05.3 — `protocol/bnet/codec.cpp` (4 kLOC)

Split per packet family into `src/protocol/bnet/src/codec/<family>.cpp`. Keep `codec.cpp` as the registry + dispatch only.

## 05.4 — `common/bnet_protocol.h` (3.6 kLOC)

Today it's one mega-header packed with every `#define PACKET_*` and POD struct. Replace with:

```
src/protocol/bnet/include/protocol/bnet/
  ids.hpp            # enum class PacketId : uint16_t
  packets/
    auth.hpp
    realm.hpp
    chat.hpp
    ...
```

Keep `src/common/bnet_protocol.h` as a deprecated forwarder for one release.

## 05.5 — Test splits

Same rule: one file per behaviour family. `codec_test.cpp` → `codec/auth_test.cpp`, `codec/chat_test.cpp`, …

## Acceptance criteria

- [ ] `git ls-files | xargs wc -l | awk '$1 > 1500 && $2 !~ /vendor/' ` is empty.
- [ ] No translation unit in `domain/`, `application/`, `infra/`, `protocol/` exceeds the soft cap from plan 01.
- [ ] Build times do not regress > 5 % (parallel compile of small TUs is usually faster).

## Risks

- Big splits churn `git blame`. Use `git log --follow` and add the split commit to `.git-blame-ignore-revs`.

## Out of scope

- Algorithmic changes inside the split units (do that in dedicated PRs after the split lands).
