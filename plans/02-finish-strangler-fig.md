# 02 — Finish the Strangler-Fig Migration

**Goal:** Reduce `src/bnetd`, `src/d2cs`, `src/d2dbs` to **empty
shells** (only thin `main()` + composition) and delete every legacy
implementation file. The `integration_legacy_*` libraries are the
landing zone.

## 1. Current state (post R209)

- `src/bnetd/` still owns ~80 `.cpp/.h` pairs (account, channel,
  clan, game, ladder, mail, prefs, sql_*, storage_*, command,
  helpfile, tournament, ipban, news, watch, …).
- `src/d2cs/` and `src/d2dbs/` are largely untouched by strangler-fig.
- `src/v3/integration/legacy_bnetd/` already houses `irc_link.cpp`,
  `handle_bnet_link.cpp`, etc.
- The `extern "C" int pvpgn_v3_<op>_try(...)` pattern is established
  and documented in `/memories/windows-tooling.md`.

## 2. Relocation order (largest blast-radius first → smallest)

Each row is one round. Each round must land GREEN.

| Round | File(s) to relocate | Approx LOC | Notes |
|-------|---------------------|------------|-------|
| R216 | `command.cpp` + `command.h` | ~6 k | Largest remaining. Same script-driven guard strip as R209. |
| R217 | `account.cpp` + `account_wrap.cpp` + `attr*.cpp` | ~5 k | Attribute layer is the deepest legacy hairball; relocate as unit. |
| R218 | `channel.cpp` + `channel_conv.cpp` | ~3 k | |
| R219 | `game.cpp` + `game_conv.cpp` + `anongame*.cpp` | ~5 k | Big, but cohesive. |
| R220 | `clan.cpp` + `team.cpp` + `tournament.cpp` + `ladder*.cpp` | ~4 k | Social/competitive cluster. |
| R221 | `friends.cpp` + `mail.cpp` + `news.cpp` + `topic.cpp` + `userlog.cpp` + `watch.cpp` | ~3 k | Misc social. |
| R222 | `storage*.cpp` + `sql_*.cpp` + `file_plain.cpp` | ~6 k | **Replace with infra adapters** (see §4). |
| R223 | `prefs.cpp` + `prefs_v3_shim.h` retirement | ~1 k | Config fully owned by `infra/config`. |
| R224 | `handle_telnet.h`/`handle_anongame.h`/`handle_bot.h`/`handle_d2cs.h`/`handle_wol.h` + remaining `handle_*.cpp` | ~2 k | All into `integration/legacy_bnetd` or already-v3 `integration/{telnet,wol,bnet}`. |
| R225 | `helpfile.cpp` + `icons.cpp` + `ipban.cpp` + `i18n.cpp` + `output.cpp` + `tracker.cpp` + `udptest_send.cpp` + `versioncheck.cpp` + `runprog.cpp` + `support.cpp` + `quota.h` + `tick.cpp` + `timer.cpp` + `realm.cpp` + `message.cpp` + `cmdline.cpp` | ~5 k | Last sweep, mostly utilities. |
| R226 | `lua*.cpp` | ~3 k | Move to `infra/lua` + `scripting/`. See `13-plugin-and-scripting.md`. |
| R227 | `server.cpp` + `server_v3_hook.cpp` + `connection.cpp` | ~3 k | Connection lifecycle is the last load-bearing wall. |
| R228 | `main.cpp` shrinks to <100 LOC composition root → moves to `src/v3/app/bnetd/main.cpp`; `src/bnetd/main.cpp` becomes a stub redirecting to it (or deleted). | — | |

After R228, `src/bnetd/` should contain only headers kept for legacy
include compatibility. Then R229–R232 do the same for `src/d2cs/`
and `src/d2dbs/`.

## 3. Mechanical playbook (repeat per round)

1. **Pick** the file.
2. **Wrap** every external call site in
   `#ifdef PVPGN_V3_BNETD_INTEGRATION` … `pvpgn_v3_<op>_try(...)`
   strangler-fig calls. (Skip if the file is internal-only.)
3. **Verify** GREEN with legacy fallback path (set
   `PVPGN_V3_BNETD_INTEGRATION=0`).
4. **Copy** the file to `src/v3/integration/legacy_bnetd/src/<file>_link.cpp`.
5. **Strip** guards with `scripts/dev/strip_v3_guards.py`
   (script created in R209, reusable).
6. **Remove** the source from `src/bnetd/CMakeLists.txt` `BNETD_LIB_SOURCES`.
7. **Add** the source to `src/v3/integration/legacy_bnetd/CMakeLists.txt`.
8. **Delete** the original `src/bnetd/<file>.cpp`.
9. **Build matrix** (`cmake --build` Debug + Release; v3-compose
   smoke; ctest).
10. **Write** `plans/r###-checklist.md` recording LOC, files, smoke
    result. (R201–R209 templates.)

## 4. Replace, don't relocate (R222)

Storage and SQL backends are the one cluster that must be **rewritten,
not moved**:

- Define `application/persistence/AccountRepository.h` etc. (see
  `07-persistence-and-migrations.md`).
- Implement adapters in `infra/persistence/{sqlite,mysql,postgres,file}`.
- Legacy callers route through the strangler-fig shim, but the
  destination is the new repository — not a `_link.cpp` copy of the
  legacy storage code.
- Delete `storage*.cpp` and `sql_*.cpp` once parity tests pass.

## 5. Exit criteria

- `find src/bnetd src/d2cs src/d2dbs -name '*.cpp'` returns ≤ 3 files
  (the three `main.cpp` thin entry points, if any).
- `cmake --target bnetd_legacy` still links (for downstream packagers
  who set `PVPGN_BUILD_LEGACY=ON`) but everything it does is delegated
  into `integration_legacy_bnetd_linked`.
- Roadmap to delete `PVPGN_BUILD_LEGACY` entirely → `14-legacy-retirement.md`.

## 6. Risks & mitigations

- **Wire compat regressions**: every relocation round must pass the
  v3-compose smoke (CLIENT_LOGINREQ1, CLIENT_JOINCHANNEL,
  CLIENT_MESSAGE round-trip). If a round breaks the smoke, revert.
- **MSVC `/WX` and Unicode**: legacy headers define `UNICODE` for
  WIN32; relocated code must keep the `setup_before.h`/`setup_after.h`
  bracket idiom (see `/memories/repo` and `00-overview.md` §3 rule 5).
- **Header em-dash trap** (see `/memories/windows-tooling.md`): use the
  tool-driven file edits, never PowerShell heredocs.
