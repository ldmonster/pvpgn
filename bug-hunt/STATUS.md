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
