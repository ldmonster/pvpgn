# PAUSE — handoff for the next Claude agent

Branch: `feat/overhaul`. Repo: `/home/cnupt/work/pvpgn` (v3 rewrite).
Oracle: `/home/cnupt/work/pvpgn-server` (original pvpgn, the ground truth).

## What this effort is

Differential bug-hunt: drive protocol-faithful mock BNCS clients against BOTH
the original pvpgn-server (oracle) AND v3, diff responses, fix each divergence.
Tracking in `bug-hunt/STATUS.md` + `bug-hunt/findings/*.md`. Tests live in
`tests/diff/`. User directive: **"make all mocked clients compatible with the
old (real) flow to test the new flow; enhance the mocks as needed."**
Full state memory: `/home/cnupt/.claude/projects/-home-cnupt-work-pvpgn/memory/diff-bug-hunt-state.md`.

## Waves 24-25 — DONE (NLS passchange + friends list)

- W24 `d01bfee`: WarCraft III NLS password change (0x55/0x56) — 0x55 reuses the
  W3 login challenge against the stored verifier; 0x56 verifies M1 and stores the
  new salt+verifier. Tests: fsm_auth_w3_test + diff_w3_passchange.py (matches oracle).
- W25 `5d3aeab`: friends list (SID_FRIENDSLIST 0x65 + /friends add|remove) — wired
  the existing AddFriend/RemoveFriend/ListFriends use-cases via a run-loop
  InMemoryFriendListRepository + 3 BnetUseCaseContext fields. diff_friends.py matches.
  GOTCHA: new context fields must be added to the full designated-init blocks in
  fsm_test.cpp / fsm_channel_test.cpp (-Werror=missing-field-initializers).

## Wave 26 — DONE (game advertise/list, commit 44a9fd3)
SID_STARTADVEX3 (0x1C) + GETADVLISTEX (0x09): wired StartGame + ListPublicGames
over the shared game repo. diff_gamelist.py matches the oracle (advertise on one
conn, visible to another). GOTCHA: GETADVLISTEX request has TWO trailing cstrings
(name+password) — the oracle aborts without the password.

## Wave 27 — DONE (/who, /whois, /users — commit e2052d7)
Implemented as FSM interceptors via a new channel_reader. diff_channelcmds.py:
/who member set matches the oracle. HARNESS NOTE: the oracle localize()s +
charset-converts INFO replies; with the mock (no codepage) the localized text is
garbled, so only /who's (non-localized) member names are byte-comparable; v3's
clean /whois is verified v3-side. /squelch still open (needs broadcast filtering).

## Wave 28 — DONE (/squelch /unsquelch + broadcast filtering, commit 5f0c986)
New IIgnoreStore + filter_squelched() drops squelched senders from TALK/EMOTE
broadcasts (mirrors MF_X). diff_squelch.py: bob heard before /squelch, suppressed
after, matches oracle. ALL BNCS chat commands now implemented.

## NEXT — remaining post-login divergences

1. WOL post-login lobby/game commands (LIST/JOIN game model, GAMEOPT, STARTG,
   matchbot) — still skeleton no-ops (see findings/wol-chat-lobby.md). This is
   now the largest remaining functional area.
2. JOINGAME password enforcement (noted while wiring game create).

Note: the CommandRegistry is still NOT wired into bnetd (make_use_case_context
doesn't set command_registry/permission_checker); the implemented chat commands
(/whisper, /me, /friends, /who, /whois, /users) are FSM interceptors instead.
Wiring the registry would be the place to add admin/operator commands later.

## Wave 23 — DONE (close WOL gap in v3 + harden + remove dead build code)

Commits: `b36c42a` (WOL auth), `cd81a66` (fuzz fix), `a1eb7a5` (dead CMake).
- WOL auth implemented in v3 → full client matrix now 19/19 oracle AND 19/19 v3
  match (OLS+NLS+WOL). New IWolCredentialStore + InMemoryWolCredentialStore;
  WolFsm CVERS/VERCHK/APGAR + try_wol_authenticate (auto-create first login,
  verbatim APGAR compare, 379/378/welcome). `--wol-port`/`--irc-port` now
  configurable (were hardcoded 4000/6667). Tests: wol_fsm_native_auth_test.cpp,
  diff_wol_login.py.
- Hardening: ASan + libFuzzer (9.7M execs) + 54 malformed-input cases → 0
  sanitizer hits. Fixed fuzz target that never compiled (core:: → pvpgn::core::).
- Dead code: removed 42 unreachable CMakeLists (orphaned pvpgn_* shadow targets);
  proven zero-impact via add_subdirectory reachability analysis + clean build+tests.

Remaining WOL work (post-login, separate from auth): the lobby/game commands
(LIST/JOIN game model, GAMEOPT, STARTG, automatch matchbot) are still skeleton
no-ops — see bug-hunt/findings/wol-chat-lobby.md. Also still open: NLS passchange
(0x55/0x56). Conservatively-kept dead files: src/common/CMakeLists.txt and
src/win32/CMakeLists.txt (pending a Windows-build check).

## Wave 22 — DONE (full supported-client mock matrix + WOL mock)

Added mock clients for PvPGN's ENTIRE supported-client matrix and a driver:
- `tests/diff/clients.py` — catalog (21 products / 149 versions / 19 paths) in 3
  families (ols/nls/wol) + a `login()` dispatcher.
- `tests/diff/wol_client.py` — faithful Westwood Online (IRC dialect) mock:
  CVERS<sku>→VERCHK→APGAR→NICK→USER→MOTD.
- `tests/diff/original_server.py` — now enables `wolv1addrs`/`wolv2addrs` (WOL is
  off by default) on port+2/port+3.
- `tests/diff/diff_all_clients.py` — boots both servers, 1 rep per path.
RESULT: 19/19 log in vs the ORACLE; OLS+NLS match v3 (8/8); WOL 0/11 vs v3 —
v3 lacks the WOL APGAR/CVERS auth (skeleton). Documented F-W22 in
findings/wol-chat-lobby.md. Run: `python3 tests/diff/diff_all_clients.py`.

Next WOL step: implement WOL auth in `src/protocol/wol/src/wol_fsm/wol_auth.cpp`
(store APGAR, SKU→clienttag, VERCHK 379 reply, USER→auto-create/verify+MOTD,
mirroring `pvpgn-server` `handle_wol_authenticate`), then the WOL rows flip to
match and add a `diff_wol_login.py`. v3 WOL/IRC ports are HARDCODED 4000/6667
(`cfg.wol_port` not parsed) — making them configurable is a prerequisite for a
clean WOL differential against v3.

## Wave 21 — DONE and committed

WarCraft III SRP-3 (NLS) running-server differential. Commits:
- v3 `fb79252` (wave 21): Python SRP-3 mock (`tests/diff/bnet_srp3.py`),
  `bncs_client.py` create_account_w3/login_w3, `diff_w3_login.py`, salt-wire fix,
  v3 e-mail-prompt (RESPONSE_EMAIL) parity, unit test, STATUS + findings.
- oracle `02c37c8` on branch `fix/bnetsrp3-safe-toupper-ub` (in pvpgn-server):
  fixes a REAL bug — `BnetSRP3::init` scrambled username/password because
  `safe_toupper(*(source++))` is a macro that double-evaluates its arg (UB).

`diff_w3_login.py` now matches the oracle on all six fields (full mutual auth).
Full v3 unit suite green; all 8 `diff_*.py` scenarios pass against the rebuilt
oracle. **The oracle binary was rebuilt with the fix** (`cmake --build build`).

## NEXT natural task (wave 22 candidate): NLS password-change (0x55 / 0x56)

`on(PassChangeRequest)` (0x55) and `on(PassChangeProofRequest)` (0x56) in
`src/protocol/bnet/src/fsm/fsm_auth.cpp` still `return ok()` (stubbed). The
oracle implements them (`_client_passchangereq` / `_client_passchangeproofreq`
in `pvpgn-server/src/bnetd/handle_bnet.cpp` ~1966-2068): same SRP-3 challenge as
login (salt + B from the stored verifier), then on a correct M1 it stores a NEW
salt+verifier supplied by the client. Add a `diff_w3_passchange.py` scenario and
the v3 handlers + store update. (See findings/nls-auth-flow.md F-W21 "Still open".)

Other open items: WOL (Westwood) clients; friends/watch list; GETADVLISTEX game
list; /who /whois /squelch; F-W18b (EID_JOIN statstring/USERFLAGS).

## Build / test / diff commands

```bash
cd /home/cnupt/work/pvpgn && source .localdeps/env.sh
cmake --build build/v3-dev -j$(nproc)                 # v3 build
ctest --test-dir build/v3-dev -L unit -j$(nproc)      # unit suite (~3143, all pass)
# diff scenarios (oracle is slow to boot; run in background, ~100-150s each):
cd tests/diff && python3 diff_w3_login.py             # has built-in defaults
python3 diff_chat.py --v3-bnetd ../../build/v3-dev/src/app/bnetd/bnetd  # others need --v3-bnetd
```
Oracle build (already built WITH the fix): `cd /home/cnupt/work/pvpgn-server &&
cmake --build build -j` (targets `common` then `bnetd`).

GOTCHAs (still current):
- The real `application_auth` target is in `src/CMakeLists.txt` (~line 924), NOT
  `src/application/auth/CMakeLists.txt`.
- `BnetUseCaseContext` is aggregate-initialized in `fsm_channel_test.cpp` and
  `fsm_test.cpp` — add new fields there too or `-Werror=missing-field-initializers`.
- Flaky-under-parallel (pass standalone, ignore): multilocale / anongame-maplist
  temp-file-race tests. `ctest` does NOT auto-build — rebuild changed test targets
  first or you get "Not Run".
- Debugging the oracle's crypto: link a probe against its own
  `build/src/common/libcommon.a build/src/compat/libcompat.a build/lib/fmt/libfmt.a`
  and call `BnetSRP3` directly. DO NOT use `#define private public` (ABI/ODR
  corruption) — add a temporary in-source `fprintf` + rebuild `common`.

## EID / protocol cheat-sheet

Chat EIDs: SHOWUSER=1 JOIN=2 LEAVE=3 WHISPER=4 TALK=5 CHANNEL=7 USERFLAGS=9
WHISPERSENT=0xa INFO=0x12 ERROR=0x13 EMOTE=0x17. Self-echo: suppressed only for TALK.
OLS auth: 0x50 AUTH_INFO seed (logon_type 0=OLS/2=W3, server_token) → 0x51
AUTH_CHECK → 0x3A LOGONRESPONSE2. W3 NLS: 0x52 create → 0x53 logon (A+name →
msg+salt+B) → 0x54 proof (M1 → response+M2). W3 proof response: 0x00 OK,
0x02 BADPASS, 0x0E EMAIL (login OK, register e-mail; versionid>=0x0D & no email).
