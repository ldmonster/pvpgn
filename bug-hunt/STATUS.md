# Bug Hunt — Status (final summary)

Reference: `/home/cnupt/work/pvpgn-server` (upstream PvPGN-PRO) vs this repo's v3
rewrite. 25 subsystems analyzed by a discovery fleet (one findings file each under
`findings/`), triaged by the orchestrator, with confirmed *implemented-but-wrong*
bugs fixed + regression-tested. Full unit suite green after every fix.

## FIXED (15 bugs across 5 commits)
| # | Bug | Commit |
|---|---|---|
| 1 | D2 `.d2s` codec read class@36/level@40 + wrong hardcore/expansion masks | wave1 9030b71 |
| 2 | D2 CharacterClass enums mis-ordered (two different wrong orders) | wave1 |
| 3 | BnetFsm chat path emitted wrong BNCS EID values (CHANNEL/INFO/JOIN/LEAVE) | wave1 |
| 4 | IRC numerics dropped the implicit nick first-param; no PONG handler | wave1 |
| 5 | Ladder ranked by wins not rating; initial rating 1500 vs 1000 | wave1 |
| 6 | Clan rank domain enum serialized/persisted inverted vs wire | wave1 |
| 7 | BNFTP downloads broken: dispatch fed the 0x02 init byte into the FSM | wave2 7b31686 |
| 8 | Config TOML keys didn't match the loader (silently dropped) + wrong defaults | wave2 |
| 9 | Channel kick/ban authorized on bare membership (privilege escalation) | wave2 |
| 10 | IP-ban loader dropped all wildcard/range/netmask bans | wave2 |
| 11 | UserName validation rejected `[CLAN]Bob`/leading-digit; allowed `.` | wave3 ee0dcb6 |
| 12 | GAMELISTREPLY omitted the 4-byte inter-game spacer dword | wave3 |
| 13 | d2dbs codec skipped RealmName → corrupted charsave blobs | wave4 50a20ae |
| (14/15) | (config = two fixes: key-reconcile + default-correct, counted as #8) | |

## Discovery coverage (25 subsystems)
crypto-hash, bnet-codec, channel-chat, ladder, anongame, gameplay, clan,
account-attributes, d2-realm, irc-wol, commands, friends-watch, moderation-ipban,
news-motd-version, bnftp-file, tournament-gameresult, config-defaults,
profile-userdata, realm-serverlist-udp, message-squelch-quota, clienttag-locale-init,
storage-formats(*killed by outage), gamelist-encoding, d2dbs-d2gs, lua-scripting,
mail-telnet. See each `findings/<name>.md` for full detail + verified-MATCHES coverage.

Verified FAITHFUL (no bug — valuable negative coverage): the BNCS wire codec
(SID/EID constants, framing, field layouts), the broken-SHA-1 hash + hash→hex,
the SRP-3 class itself, all client-tag/arch constants, the init connection-class
dispatch, friends/realm/userdata/game-record wire layouts, d2cs↔d2dbs packet
layouts, UDP datagram codes — all byte-for-byte correct; they're just not always
wired to a live handler yet.

## NOT bugs — scope gaps (features unimplemented in the rewrite; NOT auto-fixed)
The rewrite's FSM handlers are stubs in many areas; the codecs/use-cases exist but
aren't wired. These are missing features, not regressions:
- friends list/notify wiring, mutual-friend flag, watch/unwatch
- news/MOTD delivery, version-check (CheckRevision — currently always passes),
  autoupdate / SID_GETFILETIME reply
- READUSERDATA/WRITEUSERDATA, CHANGEPASSWORD, several CREATEACCOUNT FSM paths
- realm-list reply, udptest / NAT-plug detection
- squelch/ignore + flood-quota enforcement on the live talk path
- anongame result-agreement / anti-cheat + ladder update on game result
- mail system, telnet login/auth (telnet currently runs as guest acct 0 — but
  the in-memory permission checker fails closed, so it denies rather than grants)
- Lua: the bundled legacy scripts are inert under the new host (hook args reshaped)
Each is documented in its findings file with the original behaviour for whoever
implements the feature.

## DEFERRED — real divergence, likely intentional redesign / needs a product decision
- CRYPTO-1: WAR3/W3XP login wired to OpenSSL **SRP-6a**, not the legacy **SRP-3**
  (a faithful SRP-3 port exists, unconnected). If real Blizzard WAR3 clients must
  log in, this is CRITICAL; if v3 targets its own client, intentional. NOT fixed.
- GAME-1: game-type wire-code→GameType mapping wrong, but v3's GameType is a
  deliberate 5-value simplification dropping ~15 original types. Needs a decision
  on whether to restore the full clienttag-dependent table (documented in
  findings/gamelist-encoding.md Finding 7).
- ACCT Record/ladder attribute key formats differ from the original namespace
  (only matters for original on-disk/client-data interop; profile keys flagged).
- ANON inforeply `tag_unk` magic constants + DESC gametype id (findings/anongame.md).
- LADDER K-factor model (flat 32 vs tiered 50/30/20) + lround vs truncate.

## Remaining fixable implemented bugs (candidates for a future wave)
- d2dbs codec also skips charcreatetime/allowladder in GET_DATA reply (F3) and the
  charsave checksum validation (F4) — lower urgency (parallel FSM path differs).
- clienttag F1: AUTH_INFO no longer rejects disallowed `allowed_clients`.
- gamelist F3: the inchannel STARTADVEX hand-parser uses wrong offsets (but that
  path is a stub; the correct codec exists and should be wired instead).

## Runtime bug-hunt (sanitizers) — CLEAN
asan + ubsan suites rebuilt on the post-fix tree and run over the full unit suite
(3070 tests): **zero** AddressSanitizer / LeakSanitizer / UBSan reports (0 matches
for any error/leak/runtime-error/SUMMARY signature across both logs). No memory
bugs, no undefined behaviour, no leaks in the implemented code. The only test
failures under the sanitizer builds are the pre-existing config-file-loader
parallel flake (anongame_infos/maplists, icon_req, multilocale, TOML) — they
share a working directory / temp path and race under `-j`; all 40 pass 100%
serially. That flake is a test-harness issue, not a product bug (worth fixing the
loader tests to use isolated temp dirs in a future pass).

## Wave-4 discovery (8 more subsystems) + Wave-5 fixes (LANDED, commit eb122f4)
Discovery: w3-protocol, arranged-teams, anongame-matchmaking, wol-gameres,
cdkey-authcheck, malformed-input-safety, account-login-edges, channel-routing,
timer-connection-mgmt, d2cs-charlist, icons-ads-files. Most remaining gaps are
NOT-IMPLEMENTED features or the auth-handshake redesign.

Wave-5 fixes (all with regression tests; unit+functional+integration green):
- [x] DoS: unbounded IRC/telnet line buffers → capped + close (remote OOM)
- [x] Data loss: CreateAccount UID via std::hash → sequential max+1 (collision overwrite)
- [x] Channel name lookup case-sensitive → COLLATE NOCASE + folded in-memory key
- [x] WOL gameres TLV codec missing 4-byte record padding → desync fixed

## Running tally: 19 bugs fixed (waves 1-5).

## Notable CONFIRMED bugs still open (implemented-but-wrong, fixable)
- **d2cs char screen (CRIT)**: the LIVE d2cs path (FSM → d2cs_session_handler →
  make_char_list_reply) emits count+names instead of maxchar/currchar/portrait
  blocks; CREATECHARREQ/CHARLOGINREQ/DELETECHARREQ parse wrong offsets (phantom
  seqno). A byte-accurate encoder + correct structs exist but are unwired. D2
  realm character screen broken for real clients. (findings/d2cs-charlist.md)
- **icon-req loader (MED)**: no built-in default thresholds; missing/malformed
  config → all thresholds 0 → every icon unlocked (icon-switch protection
  defeated). Original always seeds defaults. (findings/icons-ads-files.md F8)
- **cdkey-authcheck (HIGH)**: AUTH_INFO sends AuthCheckReply(0x51) instead of the
  AuthInfoReply(0x50) seed; AUTH_CHECK sends no reply. Correct codecs exist,
  wrong ones wired. Tangled with the auth-handshake/SRP redesign — DEFERRED.
- **arranged-team id (MED)**: team id from std::time(nullptr) → same-second
  collisions overwrite (teams not yet wired; low live impact).

## Wave-6 fixes (LANDED, commit b36d89a) — 22 bugs fixed total
- [x] d2cs char screen (CRIT): CHARLISTREPLY/CREATECHAR/CHARLOGIN/DELETECHAR wire layouts
- [x] icon-req thresholds: seed original defaults (missing config no longer unlocks all)
- [x] arranged-team id: monotonic next_id() (was std::time → collision overwrite)

## Final discovery wave (deep codec, d2gs, email/finger) — no new fixable bugs
- deep-codec-w3-gamereport.md: **0 wire bugs** — every variable-length body
  (GAMELISTREPLY records, GAME_REPORT, STATSREPLY, W3 NLS/SRP, CLANMEMBERLIST,
  STARTGAME1/3/4) verified byte-faithful field-by-field. Strong negative coverage.
- d2gs-routing.md: d2cs↔d2gs routing NOT-IMPLEMENTED (no live path); the
  implemented register/auth bridges are correct byte-ports.
- email-finger.md: SETEMAIL/GETPASSWORD/CHANGEEMAIL handlers stubbed
  (NOT-IMPLEMENTED); email key namespace MATCHES (BNET\acct\email); /finger not
  implemented. No security issue in implemented code.

## CONCLUSION
~36 subsystems compared against upstream. **22 behavioral bugs fixed** across 6
clean commits (all unit+functional+integration green; asan/ubsan clean). The deep
final pass found no further fixable implemented bugs — remaining items are:
1. **Unimplemented features** (FSM handlers stubbed: news/MOTD, version-check,
   userdata, realm-list, friends/watch, mail, telnet-auth, squelch/quota, Lua
   hook data, anongame result-agreement+matchmaking, d2gs routing). Codecs +
   use-cases mostly exist and are byte-faithful; they just aren't wired. These
   are feature work, not regressions.
2. **Product decisions** (would change v3's own client contract / e2e):
   - auth handshake: AUTH_INFO reply + version-check + SRP-3-vs-SRP-6a login.
   - game-type enum: restore the full clienttag-dependent table vs keep the
     simplified 5-value set.
Both are documented with the original behavior in their findings files.

## Waves 7-8 (LANDED) + final discovery — 30+ distinct bugs fixed
Wave 7 (commit 3814d54): SQL injection (channel repo, CRIT), 4 SQL repos broken
vs schema (clans/friends/ladder/ip_bans, HIGH) + new real-DB integration tests,
timestamp attribute keys. Wave 8 (commit 6727834): legacy .plain account-file
read/write (CRIT — migration imported zero accounts).

Final discovery (file-storage-legacy, mysql-pg-schema, nls-auth-flow deep):
- file-storage: FIXED (wave 8).
- MySQL/PostgreSQL: NOT-IMPLEMENTED — no schema, only accounts persisted, rest
  in-memory stubs; production uses the SQLite UoW factory directly. Feature work,
  documented in findings/mysql-pg-schema.md.
- OLS login (CRIT): DEFERRED with a byte-exact FIX PLAN in findings/nls-auth-flow.md.
  Confirmed the FSM calls the wrong LoginUser overload AND no production
  IPasswordHasher is wired. Fixing it breaks the e2e (which passes only because of
  the bug: the Python client sends raw words, not a real double-hash). Needs a
  hasher adapter + composition wiring + an e2e crypto rewrite — part of the auth
  decision below.

## FINAL CONCLUSION (~40 subsystems compared, 30+ bugs fixed, 8 clean commits)
The clearly-fixable *implemented-but-wrong* bug surface is exhausted. asan/ubsan
clean; unit+functional+integration green after every wave. Remaining work is two
buckets, both needing a decision rather than a bug-fix:

1. **Auth subsystem (one coherent decision).** WAR3 login uses SRP-6a not legacy
   SRP-3; OLS login calls the wrong hash overload; no production password-hasher
   is wired; version-check always passes. ALL of this hinges on one question:
   **must stock Blizzard clients connect?** If yes, it's the top priority and the
   plans are recorded (nls-auth-flow.md FIX PLAN + crypto-hash.md). If v3 targets
   its own client, much of it is intentional. NEEDS USER DECISION.

2. **Unimplemented features** (stubbed FSM handlers + MySQL/PG persistence): news/
   MOTD, version-check, userdata r/w, realm-list, friends/watch, mail, telnet-auth,
   squelch/quota, matchmaking, d2gs routing, WOL chat/lobby, Lua hook data,
   anongame result-agreement. Codecs/use-cases mostly exist and are byte-faithful;
   they just aren't wired. Feature work, each documented with original behavior.

## Waves 9-10: new bug CLASSES (races) + the auth headline fix
Pivoted from subsystem-comparison to two classes comparison can't find:

Wave 9 (concurrency, commit c173062): bnetd runs asio on N worker threads.
Fixed 1 CRIT + 2 HIGH races — shared SQLite connection across threads (→ per-UoW
connections + busy_timeout), unsynchronized InMemorySessionRegistry (→ shared_mutex)
and InMemoryEventBus (→ mutex, handlers invoked outside the lock). Each with a
concurrency smoke test.

Wave 10 (commit a9144a7): **fixed the CRITICAL OLS login bug — stock Battle.net
clients can now log in.** BnetSessionHasher (real broken-SHA-1 double-hash) +
FSM session-hash overload + main.cpp wiring + e2e Python broken-SHA-1 port.
Verified end-to-end: all 3 e2e journeys pass through the real bnetd, check-all 18/0/0.

## Domain-invariant findings (fixable, not yet applied)
From invariants-game-channel.md + invariants-clan-account-ladder.md:
- Clan: promote accepts any rank → can reach TWO or ZERO chieftains (should clamp
  Peon..Shaman + atomic crown transfer); IClanRepository has no find_by_account so
  a member can join two clans.
- Channel: JoinChannel never leaves the previous channel → user in two channels at
  once; no per-channel operator model (creator isn't made operator; moderated flag
  ignored); max==0 means unlimited in v3 vs admin-only in original.
- Ladder: LadderEntry.rating is signed with no floor → a losing streak can drive it
  negative (original clamps to ≥1); but the apply path is unwired so latent.
- Game: a 1-player InProgress game isn't reported+destroyed (original drops <2).

## REMAINING auth piece: SRP-3 NLS (WAR3/W3XP)
The OLS path (StarCraft/Diablo II/classic) is DONE. WAR3/W3XP use SID_AUTH_ACCOUNTLOGON
/PROOF, which v3 wires to OpenSSL SRP-6a (nls.cpp) instead of the legacy SRP-3 (a
faithful bnet_srp3.cpp exists, unconnected). Routing the WAR3 opcodes through SRP-3
is the remaining auth work (no e2e harness for NLS yet — verify via SRP-3 protocol
unit vectors). See crypto-hash.md.

## Wave 11 (commit 100acb9): domain-invariant fixes
Clan exactly-one-chieftain (promote rejects Chieftain + atomic transfer_chieftain);
channel leave-previous-on-join (no more user-in-two-channels); ladder rating floor
(kMinRating=1 clamp). Suite 3136/3136.

## SRP-3 NLS path — scoped: it's a multi-layer FEATURE, not an adapter swap
Investigated the WAR3/W3XP (SID_AUTH_ACCOUNTLOGON/PROOF) path. It is incomplete at
THREE layers, not just "wrong crypto":
1. The bnetd FSM has no SID_AUTH_ACCOUNTLOGON / ACCOUNTLOGONPROOF handlers wired
   (none in fsm_auth.cpp) — the NLS use-case + adapter exist in bnetd_service but
   nothing drives them from the wire.
2. CreateAccount does NOT compute/store an NLS verifier + salt, so even a wired
   NLS login has nothing to verify against.
3. The adapter is SRP-6a (nls.cpp), not the legacy SRP-3 (bnet_srp3.cpp exists,
   unconnected).
Making WAR3 login work end-to-end therefore means implementing the NLS feature
(handlers + verifier-at-creation + SRP-3 adapter), and there is NO WAR3 client /
NLS e2e harness here to validate it — only deterministic SRP-3 unit vectors.
Recommendation: a focused, unit-test-verified NLS/SRP-3 implementation as its own
effort. The OLS path (StarCraft / Diablo II / classic) is DONE and e2e-verified.

## RUNNING TOTAL: ~37 distinct bugs fixed across 11 waves. check-all 18/0/0.

## Wave 12 (commit 3f76549): differential testing harness + the 0x01 init-byte fix
Built BNCS mock clients (tests/diff/) that drive BOTH the upstream pvpgn-server
(built it: cmake -DWITH_BNETD=ON -DWITH_D2CS=OFF -DWITH_D2DBS=OFF -DWITH_LUA=OFF)
AND v3, diffing behaviour against the oracle. This is the verification mechanism
the NLS/SRP-3 work needed.

It immediately found a CRITICAL real-client bug: v3 only recognised a BNet
connection by a leading 0xFF, but every real client (and the original) sends a
0x01 CLIENT_INITCONN_CLASS_BNET octet first -> v3 hung. **So real clients could
not connect at all, even after the OLS password fix.** Fixed the dispatch (strip
0x01, like the 0x02 BNFTP fix). Now v3's OLS login is OUTCOME-EQUIVALENT to the
oracle: accept 0x00 / wrong-pw 0x02 / unknown 0x01 all match, with the real
broken-SHA-1 double-hash + server token. Added a real-client-init-byte e2e journey.

This makes the OLS login genuinely real-client-compatible (transport + crypto),
verified against ground truth. Remaining (oracle-confirmed) gap: the AUTH_INFO
seed (server_token) + the AUTH_CHECK step (cdkey-authcheck findings) and the
NLS/SRP-3 path — all now have a differential harness to verify against.

## RUNNING TOTAL: ~39 distinct bugs across 12 waves. Differential oracle harness in place.

## Wave 13 (commit 05db4a8): differential chat harness + empty-roster fix
Extended the mock-client harness (tests/diff/diff_chat.py) to the post-login
chat/channel flow. It found that on channel join v3 emitted only [CHANNEL] while
the oracle emits [CHANNEL,INFO,USERFLAGS,SHOWUSER,...] — v3 skipped the joining
user, so a real client saw an EMPTY channel roster (not even itself). Fixed
BnetFsm join to emit USERFLAGS+SHOWUSER for every member incl. self; the diff now
matches the oracle on the key events. e2e updated to assert self-in-roster.

## Differential harness now covers: OLS login + chat/channel join. Reusable for
## whisper, /commands, game-list, friends, and the NLS/SRP-3 path next.
## RUNNING TOTAL: ~41 distinct bugs across 13 waves.

## Wave 14: differential talk harness + cross-session broadcast fix
Extended the mock-client harness (tests/diff/diff_talk.py) to two-client channel
TALK. It found CRITICAL: Alice's SID_CHATCOMMAND reached Bob on the oracle but
v3 delivered nothing — channel chat between clients was entirely broken.

Root cause: BnetFsm::broadcast_chat_event double-wrapped the SID_CHATEVENT packet
(encode(ChatEvent) already begins+finalizes), so the redundant finalize failed
and the early-return dropped EVERY broadcast before the router was called. A
second latent gap: bnetd never constructed/wired a MessageRouterImpl, so the FSM
context had a null router. Fixed both — encode() is now called directly, and
main.cpp builds + wires MessageRouterImpl with per-session egress register/unregister
in the BNet dispatch. diff_talk.py now matches the oracle (Bob gets EID_TALK).
Regression locked by fsm_channel_broadcast_test.cpp (2 real logins, asserts the
router receives a parseable EID_TALK packet for the other session).

## Differential harness now covers: OLS login + chat/channel join + two-client TALK.
## RUNNING TOTAL: ~42 distinct bugs across 14 waves.

## Wave 15: differential whisper harness + /whisper implementation
Extended the harness (tests/diff/diff_whisper.py) to private messaging. Found
HIGH: v3 never implemented the /whisper command family (/w /msg /m /whisper) —
`/w` fell through to the generic command dispatch and returned "Unknown command",
so private messages were silently dropped while the oracle delivered them.

Implemented whisper in BnetFsm::on(ChatCommand) (intercepted before the generic
dispatch, since it routes to another session rather than returning reply text):
new handle_whisper resolves target by name->account->session and routes
EID_WHISPER(0x04) to the target via the message router + EID_WHISPERSENT(0x0a)
ack to the sender; offline target -> EID_ERROR(0x13). diff_whisper.py now matches
the oracle byte-for-byte. Unit guards added (routing, all 4 aliases, offline path).

## Differential harness now covers: OLS login + chat/channel join + two-client
## TALK + private WHISPER. Reusable for /commands, game-list, friends, NLS/SRP-3.
## RUNNING TOTAL: ~43 distinct bugs across 15 waves.

## Wave 16: differential channel-part-on-disconnect + EID_LEAVE fix
Extended the harness (tests/diff/diff_leave.py) to channel part on disconnect.
Found HIGH: when a user dropped their connection v3 broadcast no EID_LEAVE, so
the remaining members kept a ghost roster entry (the original announces the part).

Root cause: EID_LEAVE was only emitted from the explicit SID_LEAVECHAT handler; the
disconnect path went through LogoutUser, which does the membership cleanup but
`(void)`-discards the members_to_notify. Fix: new BnetFsm::on_disconnect() invoked
from the dispatch on_close before LogoutUser, reusing the on(LeaveChannel) path so
remaining members get EID_LEAVE (LogoutUser then no-ops -> single broadcast).
diff_leave.py matches the oracle (bob: [(3,'alice','')]); unit guards added.

Also UNCOVERED (finding F-W16b, fix pending): InMemoryChannelRepository never
assigns channel ids, so every created channel keeps id 0 -> distinct channels
collide in by_id_[0], and 0 is ambiguous vs the FSM's no-channel sentinel. Next
scenario: assign monotonic channel ids (reserve 0) + propagate through JoinChannel.

## Differential harness now covers: OLS login + chat/channel join + two-client
## TALK + private WHISPER + channel part-on-disconnect (EID_LEAVE).
## RUNNING TOTAL: ~45 distinct bugs across 16 waves.

## Wave 17: channel id assignment — distinct channels no longer collide
Fixed finding F-W16b (uncovered in wave 16). InMemoryChannelRepository stored
each channel verbatim under channel.id().value(); since JoinChannel creates
channels with the id-0 "assign on persist" sentinel, every new channel landed in
by_id_[0] -- "RED" and "BLUE" collided and a user joining one resolved to the
other's object. Fix: the repo now allocates a monotonic id (>=1) for any channel
saved with id 0 (new domain helper Channel::with_id stamps it on), reserving 0 as
the FSM no-channel sentinel; JoinChannel's existing re-read-by-name propagates the
assigned id. diff_multichannel.py matches the oracle (Carol joining RED sees
{alice,carol}, never bob in BLUE). Unit guards added for the repo.

## Differential harness now covers: OLS login + chat/channel join + two-client
## TALK + private WHISPER + part-on-disconnect + multi-channel isolation.
## RUNNING TOTAL: ~45 distinct bugs across 17 waves (F-W16b now closed).

## Wave 18: /me (/emote) channel emotes + TALK/EMOTE self-echo asymmetry
Extended the harness (tests/diff/diff_emote.py) to emotes. Found v3 never
implemented /me — it was rejected as an unknown command, dropping the emote.
Implemented handle_emote (broadcast EID_EMOTE to others + echo to self). Key
nuance verified against the oracle: the server suppresses the speaker only for
TALK (channel.cpp:734); EMOTE/WHISPER are echoed back to the sender. v3's TALK
already excludes self (correct); emote now echoes to self. diff_emote matches.
Also logged F-W18b (deferred): EID_JOIN omits the joiner's statstring/USERFLAGS.

## Differential harness now covers: OLS login + chat/channel join + TALK +
## WHISPER + part-on-disconnect + multi-channel isolation + /me EMOTE.
## RUNNING TOTAL: ~47 distinct bugs across 18 waves.

## Wave 19: faithful AUTH_INFO (0x50) seed — real-client OLS handshake
Goal (user): make the mock clients faithful to the real "old flow" so they test
the new flow. Doing so exposed CRITICAL: v3's on(AuthInfo) jumped straight to the
SID_AUTH_CHECK (0x51) result and NEVER sent the SID_AUTH_INFO (0x50) seed. Our
adaptive mock hid it, but a real client blocks for that seed (server token for the
password double-hash + logon-type flag selecting OLS vs W3/NLS). So no real client
could authenticate.

Fix: on(AuthInfo) now sends the AuthInfoReply seed (logon-type 2 for WAR3/W3XP
else 0, per-session nonzero server_token, MPQ name + CheckRevision equation);
on(AuthCheckRequest) now sends the 0x51 result. Mock clients (diff bncs_client +
both e2e tests) were made strictly faithful (require the seed, use its token).
diff_ols_login matches on seed_present/server_token_nonzero/logon_type + outcomes;
both e2e journeys pass; unit guards added/updated (fsm_auth_create_login, fsm_test).

NLS still open (F-W19): the seed now advertises logon-type 2 for WAR3/W3XP but the
0x53/0x54 SRP handlers are stubbed + no verifier at creation — a real WC3 client
would stall at 0x53. Wiring bnet_srp3 + LoginUserNls + credential store is next.

## RUNNING TOTAL: ~48 distinct bugs across 19 waves. Real OLS clients can now
## complete the auth handshake against v3 (verified vs oracle + faithful mocks).

## Wave 20: WarCraft III SRP-3 (NLS) login implemented + wired
The 0x53/0x54 handlers were stubbed and there was no verifier at account
creation, so real WAR3/W3XP clients (which get logon-type 2 from the wave-19
seed) stalled at 0x53. Now implemented end to end: new LoginUserW3 use-case +
ISrp3CredentialStore (in-memory impl), FSM handlers for 0x52 (create: store
salt+verifier), 0x53 (challenge: salt+B, hold M1/M2), 0x54 (verify M1, return M2,
attach session, LoggedIn). Uses the parity-verified BnetSrp3 (32-byte modulus)
with the original's exact wire block-size conventions. Wired into live bnetd.

Verified by a full create→login→proof C++ round-trip (fsm_auth_w3_test.cpp) with
a BnetSrp3 *client* — same bit-exact crypto the original uses — both sides derive
the same K and M2 matches; wrong-proof→BadPass, unknown→failure. 3141 tests pass.
Open: a Python differential mock (needs SRP-3 + BigUInt legacy conversions ported
to Python, golden-verified) for running-server parity; NLS passchange (0x55/0x56).

## RUNNING TOTAL: ~49 distinct bugs/features across 20 waves. OLS real-client
## handshake + WarCraft III SRP-3 login now implemented & tested.

## Wave 21: WarCraft III SRP-3 (NLS) running-server differential
Built the running-server differential for W3 NLS (wave 20 only had a self-
consistent C++ round-trip). A golden-verified Python SRP-3 (tests/diff/bnet_srp3.py)
drives bncs_client.py's new create_account_w3/login_w3 through diff_w3_login.py
against BOTH servers. Driving the *running oracle* exposed three things:

1. [mock BUG, fixed] salt_to_wire used block-4 LITTLE-endian; the server decodes
   salt block-4 then derives raw_salt block-1 BE for the x/proof hash. Block-4
   little is not the inverse of the server's block-4 decode → salt/raw_salt/x/M1
   desynced and BOTH servers returned proof_response 2. Fixed to block-4 BIG-endian
   (verified to round-trip through from_bytes_legacy(.,4,false)). v3 then completed
   mutual auth.

2. [REAL BUG in the ORIGINAL, fixed in the oracle] With the mock fixed, v3 passed
   but the oracle still rejected. A C++ probe against the oracle's own BnetSRP3
   showed it hashes a SCRAMBLED username/password: BnetSRP3::init does
   `*(symbol++) = safe_toupper(*(source++))`, but safe_toupper is a MACRO that
   evaluates its arg twice → `*(source++)` advances 2x/char, scrambling the name
   AND reading past the malloc buffer (UB). So the oracle's W3 verifier + M1/M2
   are corrupt; no real WC3 client (correct username) could mutually auth. v3 is
   correct. Fixed in the oracle (user decision: genuine upstream bug) with indexed
   access `symbol[i] = safe_toupper(source[i])` in both loops → oracle verifier
   now equals v3/Python bit-for-bit.

3. [v3 gap, implemented] After both SRP-3 sides matched, the oracle returned
   proof_response 0x0E (RESPONSE_EMAIL: login OK, please register an e-mail) for
   versionid >= 0x0D accounts with no e-mail; v3 returned 0x00. Implemented in v3:
   BnetFsm stores version_id_ from AUTH_INFO; on(LogonProofW3Request) returns
   kLogonProofW3ResponseEmail when version_id_ >= 0x0D and the account has no
   e-mail (always true for the W3 path: Account carries no e-mail, no SETEMAIL
   flow). Still a successful login (M2 returned, session attached, LoggedIn).

Verified: diff_w3_login.py matches on all six fields (logon_type=2, auth_check=0,
create=0, login_msg=0, proof_response=14, m2_matches=True) → "WarCraft III SRP-3
login matches the oracle (mutual auth OK)". New unit case in fsm_auth_w3_test.cpp
(version 0x1A → 0x0E + matching M2 + LoggedIn); wave-20 round-trip (version 0)
still → 0x00. Full v3 unit suite green (3143 tests). See findings/nls-auth-flow.md
(F-W21). Oracle fix is a separate commit in /home/cnupt/work/pvpgn-server.

## RUNNING TOTAL: ~51 distinct bugs/features across 21 waves (incl. 1 real bug
## found IN the original oracle). WarCraft III SRP-3 login now differentially
## verified against the (now-fixed) running oracle, full mutual auth.

## Wave 22: full supported-client mock matrix + WOL mock
Built mock clients covering PvPGN's ENTIRE published supported-client matrix and
a driver that logs each in against the oracle (and v3 where supported):

- tests/diff/clients.py — catalog of every supported product+version (21
  products / 149 versions / 19 distinct protocol paths) across 3 login families:
  ols (STAR/SEXP/W2BN/DRTL/D2DV/D2XP), nls (WAR3/W3XP), wol (WCHT/RALT/RAL2/TSUN/
  TSXP/YURI/RNGD/NOXX/NOXQ/DN2K/EBFD), with tag/arch/SKU + a login() dispatcher.
- tests/diff/wol_client.py — faithful Westwood Online mock (IRC dialect on the
  wol listener): CVERS<sku> → VERCHK → APGAR<pwtoken> → NICK → USER → welcome/MOTD,
  exactly as handle_wol.cpp expects (APGAR opaque + auto-create on first login).
- tests/diff/original_server.py — enables wolv1addrs/wolv2addrs on derived test
  ports (WOL is off by default in pvpgn) so the oracle accepts WOL.
- tests/diff/diff_all_clients.py — boots both servers, drives one representative
  per protocol path.

Result: all 19 paths log in against the ORACLE (19/19); OLS+NLS match v3 (8/8);
all 11 WOL paths FAIL against v3 (0/11) — a running confirmation of the existing
skeleton finding: v3's wol_auth.cpp uses IRC NICK/USER/PASS and ignores the WOL
APGAR/CVERS flow, so no Westwood client can authenticate. Documented as F-W22 in
findings/wol-chat-lobby.md (fix = implement WOL auth in wol_auth.cpp; larger
feature, deferred). The OLS/NLS mocks are now exercised for every supported tag.

## RUNNING TOTAL: ~52 distinct bugs/features across 22 waves. Mock clients now
## cover the FULL supported-client matrix (OLS + NLS + WOL), all logging in
## against the oracle; OLS/NLS verified equivalent on v3, WOL gap confirmed.

## Wave 23: close the WOL gap in v3 + harden + remove dead build code
Three threads, all verified:

1. WOL auth implemented in v3 (closes F-W22, 0/11 → 11/11). New IWolCredentialStore
   port + InMemoryWolCredentialStore (mirror the SRP-3 store). WolFsm gains a
   WolAuthDeps ctor + CVERS/VERCHK/APGAR handlers + try_wol_authenticate():
   auto-create on first login (store APGAR), verbatim-compare thereafter, send
   welcome/MOTD — mirroring handle_wol_authenticate. VERCHK→379, wrong token→378.
   Wired into the live WOL listener. Also made --wol-port/--irc-port configurable
   (were hardcoded 4000/6667 so two bnetd could never coexist — real robustness
   fix). Tests: wol_fsm_native_auth_test.cpp (6 cases) + diff_wol_login.py
   (auto-create/correct/wrong vs oracle). diff_all_clients.py now 19/19 oracle
   AND 19/19 v3 match (OLS+NLS+WOL).

2. Runtime hardening: built v3 under AddressSanitizer + ran the libFuzzer codec
   target (9.7M execs, 0 crashes) + a 54-case malformed-input runtime stress
   (BNCS framing/truncation, out-of-order opcodes, wrong protocol bytes, WOL
   malformed lines, per-SID garbage, W3 SRP-3 edge cases incl. A=0/A=N). ZERO
   sanitizer hits / crashes / hangs — the decoders' static bounding and the FSM
   guards hold up at runtime. Fixed one real defect: the fuzz target never
   compiled in fuzzing mode (unqualified core:: vs pvpgn::core::), silently
   disabling CI fuzzing.

3. Dead code: removed 42 never-processed (unreachable) CMakeLists — the orphaned
   pvpgn_* "shadow" target graph that double-compiled modules already defined in
   the monolithic src/CMakeLists.txt. Proven zero-impact by reachability analysis
   + clean reconfigure/build/full unit suite. (src/application/ was never even
   add_subdirectory'd.)

## RUNNING TOTAL: ~54 distinct bugs/features across 23 waves. v3 now authenticates
## EVERY supported client family (OLS + NLS + WOL) identically to the oracle;
## runtime-hardened (0 sanitizer hits across fuzzing + 54 malformed cases); dead
## shadow-CMake graph removed.

## Wave 24: WarCraft III NLS password change (0x55/0x56)
on(PassChangeRequest)/on(PassChangeProofRequest) were stubs. Implemented mirroring
the oracle: 0x55 runs the same SRP-3 challenge as login against the stored
verifier (reuse LoginUserW3::challenge) → ACCEPT+salt+B; 0x56 verifies M1, then
stores the NEW salt+verifier via ISrp3CredentialStore::store → OK+M2 (BadPass on
wrong proof; no e-mail prompt on this path). Reuses the w3_* challenge state.
Tests: fsm_auth_w3_test.cpp (change→relogin-new-pass + wrong-old-proof→BadPass) +
bncs_client.passchange_w3 + diff_w3_passchange.py (matches oracle: change OK, new
pass logs in, old pass rejected).

## Wave 25: friends list (SID_FRIENDSLIST 0x65 + /friends add|remove)
Friends opcodes were decoded but the FSM handlers were stubs and the
AddFriend/RemoveFriend/ListFriends use-cases (already present) were unwired.
Wired them via a run-loop InMemoryFriendListRepository + 3 new
BnetUseCaseContext fields (main.cpp, no BnetdService change). on(FriendsListRequest)
builds the reply from ListFriends (name + FRIENDSTATUS location); new handle_friends
intercepts /friends (/f) add|remove → AddFriend/RemoveFriend + SID_FRIENDADD/DEL ack.
protocol_bnet gains application_social/domain_social deps. Tests: diff_friends.py
matches oracle (empty→[bob]→empty). Added the 3 fields to the fsm_test/fsm_channel_test
designated-init sites (-Werror=missing-field-initializers).

## Wave 26: game advertise/list (SID_STARTADVEX3 0x1C / GETADVLISTEX 0x09)
Game opcodes were decoded but the FSM handlers were stubs (on(StartGame4Request)
just set InGame; on(GameListRequest) always sent an empty list). Wired the
existing StartGame + ListPublicGames use-cases over the shared InMemoryGameRepository
(new list_public_games BnetUseCaseContext field; main.cpp). on(StartGame4Request)
now registers the hosted game; on(GameListRequest) enumerates public games. Mock:
bncs_client.advertise_game (status 0x10 = oracle INIT_VALID public/open) + game_list.
diff_gamelist.py matches the oracle (empty → [DiffGame] visible to another client).
Faithfulness fix: GETADVLISTEX carries TWO trailing cstrings (name+password); the
original aborts without the password (v3 reads only name, tolerates trailing).
Added list_public_games to the two designated-init test sites.

## RUNNING TOTAL: ~57 distinct bugs/features across 26 waves. Login (all families)
## + NLS passchange + friends list + game advertise/list now match the oracle.
## Still stubbed: /who /whois /squelch (CommandRegistry not wired in bnetd), WOL
## post-login lobby/game commands, JOINGAME password enforcement.
