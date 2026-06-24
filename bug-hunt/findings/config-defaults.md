# Bug-hunt: Configuration defaults + limits (ORIGINAL vs v3)

Subsystem: configuration default values & limits that change server behavior.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
  - Compiled fallbacks: `src/bnetd/prefs.cpp` (`conf_setdef_*` bodies), constants in
    `src/common/setup_before.h` and `src/common/field_sizes.h`.
  - Shipped config: `conf/bnetd.conf.in`, `conf/d2cs.conf.in`.
- CURRENT v3: `/home/cnupt/work/pvpgn`
  - Compiled fallbacks: struct initializers in
    `src/infra/config/include/infra/config/server_config.hpp`.
  - Loader (key names): `src/infra/config/src/server_config.cpp`.
  - Shipped config: `conf/bnetd.toml.in`, `conf/d2cs.toml.in`.

IMPORTANT mechanics: in v3 the loader uses `get_or(key, struct_default)`. So if the
shipped TOML uses a *different key name* (or a *different section*) than the loader
reads, the operator-visible value in the shipped `.toml.in` is **silently ignored** and
the compiled struct default is used instead. Several findings below are exactly this
class of bug (shipped value is dead), which is more dangerous than a plain default
mismatch because the operator believes the shipped value is in effect.

Read-only audit. No source edited.

---

## SUMMARY OF FINDINGS

| # | Severity | Class | Item | Divergence |
|---|----------|-------|------|------------|
| 1 | HIGH | BUG | `[clan]` TOML keys | shipped `newer_time/max_members/channel_default_private/min_invites` never read (loader wants `clan_*`) → clan_max_members falls back to **100** not 50 |
| 2 | HIGH | BUG | `[irc]` TOML keys | shipped `addrs/network_name/latency` never read (loader wants `irc_*`) |
| 3 | HIGH | BUG | `[wol]` TOML keys | shipped `timezone/longitude/latitude/autoupdate_*` never read (loader wants `wol_*`) |
| 4 | HIGH | BUG | `[ladder]`/`[status]` TOML keys | shipped `war3_update_secs/update_secs/xml_output` never read |
| 5 | HIGH | BUG | `[network]` misplaced keys | `servaddrs/w3routeaddr/max_connections/packet_limit/max_conns_per_IP/max_concurrent_logins/initkill_timer` placed in wrong section/name → dead |
| 6 | HIGH | BUG | `[tracking]` `trackaddrs` | loader reads `trackserv_addrs`; shipped tracker list dead |
| 7 | MED | BUG | `[account]`/`[policy]` section split | `mail_*` shipped in `[policy]`, `hashtable_size`/`max_friends` shipped in `[account]`; loader reads them from the other section → dead |
| 8 | MED | BUG | `passfail_bantime` | shipped `300` in `[policy]` is read, but compiled struct default is **0** vs original compiled **300** |
| 9 | MED | UNSURE | `max_friends` compiled default | v3 struct **25** vs original `MAX_FRIENDS`=**20** (shipped TOML=20 but in wrong section, see #7) |
| 10 | MED | UNSURE | `clan_max_members` compiled default | v3 struct **100** vs original `CLAN_DEFAULT_MAX_MEMBERS`=**50** |
| 11 | MED | UNSURE | `clan_newer_time` compiled default | v3 struct **0** vs original `CLAN_NEWER_TIME`=**168** |
| 12 | LOW | UNSURE | `max_connections` compiled default | v3 struct **4096** vs original `BNETD_MAX_SOCKETS`=**1000** |
| 13 | LOW | UNSURE | `irc_latency` compiled default | v3 struct **300** vs original `BNETD_IRC_LATENCY`=**180** |
| 14 | LOW | UNSURE | `userflush` compiled default | v3 struct **3600** vs original `BNETD_USERFLUSH`=**1000** |
| 15 | LOW | UNSURE | `war3_ladder_update_secs`/`output_update_secs` compiled | v3 struct 3600/300 vs original 0/0 |
| 16 | LOW | UNSURE | `sync_on_logoff` compiled default | v3 struct **true** vs original **false** |
| 17 | LOW | UNSURE | `mail_quota` compiled default | v3 struct **10** vs original `BNETD_MAIL_QUOTA`=**5** |
| 18 | INFO | UNSURE | net.timeouts new model | new per-protocol idle timeouts (300s) have no original analogue; can close idle BNCS clients the original kept alive |
| 19 | LOW | UNSURE | d2cs `idletime` compiled | v3 struct **3600** vs original `MAX_CLIENT_IDLETIME`=**1800** (both ship 3600) |

Key MATCHES (good coverage) listed at the bottom.

---

## FINDING 1 — [clan] TOML keys do not match loader → shipped clan limits are dead (HIGH, BUG)

ORIGINAL compiled fallbacks (`src/common/setup_before.h:163-168`):
```
const unsigned CLAN_NEWER_TIME = 168;
const unsigned CLAN_DEFAULT_MAX_MEMBERS = 50;
const unsigned CLAN_DEFAULT_MIN_INVITES = 2;
```
Original shipped (`conf/bnetd.conf.in`): `clan_newer_time = 0`, `clan_max_members = 50`,
`clan_min_invites = 2`.

v3 loader reads (`src/infra/config/src/server_config.cpp:399-402`):
```cpp
sc.clan.clan_newer_time              = u32(*sec, "clan_newer_time", ...);
sc.clan.clan_max_members             = u32(*sec, "clan_max_members", ...);
sc.clan.clan_channel_default_private = sec->get_or<bool>("clan_channel_default_private", ...);
sc.clan.clan_min_invites             = u32(*sec, "clan_min_invites", ...);
```
v3 shipped (`conf/bnetd.toml.in:408-417`):
```
[clan]
newer_time              = 0
max_members             = 50
channel_default_private = 0
min_invites             = 2
```
Divergence: TOML keys are unprefixed (`max_members`), loader keys are `clan_`-prefixed
(`clan_max_members`). The shipped values are never read → falls back to the struct
defaults in `server_config.hpp:287-290`:
`clan_newer_time=0`, `clan_max_members=100`, `clan_channel_default_private=false`,
`clan_min_invites=2`.

Net behavior change: **max clan members defaults to 100 instead of 50** even with the
shipped conf in place. `clan_newer_time` happens to coincide (0). The struct fallback for
`clan_max_members` (100) also itself differs from the original compiled fallback (50).

Proposed fix: make the loader keys and the shipped TOML keys agree. Either rename the
TOML keys to `clan_*` or change the loader to read the unprefixed keys under `[clan]`.
Also set the v3 struct default `clan_max_members = 50` and `clan_newer_time = 168` to
match the original compiled fallbacks for the no-conf case.

---

## FINDING 2 — [irc] TOML keys do not match loader (HIGH, BUG)

v3 loader (`server_config.cpp:358-360`) reads `"irc_addrs"`, `"irc_network_name"`,
`"irc_latency"`. v3 shipped (`conf/bnetd.toml.in:366-374`) provides:
```
[irc]
#addrs        = ":6667"
network_name  = "PvPGN"
latency       = 180
```
Keys `network_name` / `latency` / `addrs` ≠ loader's `irc_network_name` /
`irc_latency` / `irc_addrs`. Shipped `latency=180` is ignored → struct default
`irc_latency=300` used (`server_config.hpp:261`). Original compiled
`BNETD_IRC_LATENCY=180` (`setup_before.h:212`); original shipped `#irc_latency = 180`.

Net behavior change: IRC ping timeout becomes 300s instead of original 180s.

Proposed fix: align key names (prefix the TOML keys with `irc_`, or strip the prefix in
the loader for `[irc]`). Set struct default `irc_latency = 180`.

---

## FINDING 3 — [wol] TOML keys do not match loader (HIGH, BUG)

v3 loader (`server_config.cpp:338-348`) reads `wol_timezone`, `wol_longitude`,
`wol_latitude`, `wol_autoupdate_serverhost`, `wol_autoupdate_username`,
`wol_autoupdate_password` (plus `apireg_addrs`, etc.). v3 shipped
(`conf/bnetd.toml.in` `[wol]`) provides `timezone`, `longitude`, `latitude`,
`autoupdate_serverhost`, `autoupdate_username`, `autoupdate_password` (unprefixed).
All of these shipped values are silently ignored.

Proposed fix: align key names between the loader and the shipped TOML.

---

## FINDING 4 — [ladder] and [status] TOML keys do not match loader (HIGH, BUG)

v3 loader (`server_config.cpp:377-378, 388-389`) reads:
`war3_ladder_update_secs`, `XML_output_ladder`, `output_update_secs`,
`XML_status_output`.
v3 shipped (`conf/bnetd.toml.in:388-401`):
```
[ladder]
war3_update_secs  = 300
xml_output        = false
[status]
update_secs   = 60
xml_output    = false
```
Keys do not match → shipped 300 / 60 ignored. Struct defaults used:
`war3_ladder_update_secs=3600`, `output_update_secs=300`
(`server_config.hpp:273, 280`). Original shipped 300 and 60
(`conf/bnetd.conf.in:590, 606`); original compiled fallbacks are both 0
(`prefs.cpp:2836, 2857`).

Net behavior change: ladder file refresh 3600s vs intended 300s; status refresh 300s
vs intended 60s.

Proposed fix: align key names; set struct defaults to match original compiled (0/0) or
the intended shipped (300/60) — pick one and make loader + TOML + struct consistent.

---

## FINDING 5 — [network] section carries keys the loader reads elsewhere (HIGH, BUG)

v3 shipped `[network]` (`conf/bnetd.toml.in:299-323`) contains:
`servaddrs`, `w3routeaddr`, `max_connections`, `packet_limit`,
`max_conns_per_IP`, `max_concurrent_logins`, `initkill_timer`.

But the loader reads:
- `bnetdserv_addrs` and `w3route_addr` from `[network]`
  (`server_config.cpp:309-310`) — NOT `servaddrs` / `w3routeaddr`.
- `max_connections`, `packet_limit`, `max_conns_per_IP`, `max_concurrent_logins`
  from `[policy]` (`server_config.cpp:254-261`).
- `initkill_timer` from `[timing]` (`server_config.cpp:228`).

Consequences with the shipped conf:
- `servaddrs = ":"` and `w3routeaddr = "0.0.0.0:6200"` are ignored → bnetd listen
  address falls back to struct default `bnetdserv_addrs="0.0.0.0:6112"` (OK by luck) and
  `w3route_addr=""` (empty → W3 route disabled, vs intended 0.0.0.0:6200).
- `max_connections = 1000` (shipped) ignored → struct default **4096**
  (`server_config.hpp:177`). Original compiled `BNETD_MAX_SOCKETS=1000`.
- `packet_limit = 1000` ignored → struct default **0** (unlimited)
  (`server_config.hpp:178`). Original compiled `BNETD_PACKET_LIMIT=1000`.
- `max_conns_per_IP`, `max_concurrent_logins`, `initkill_timer` shipped values ignored
  (struct defaults 0/0/0; `initkill_timer` shipped 120 lost).

Net behavior change: connection cap 4096 instead of 1000; packet queue limit disabled
instead of 1000; W3 route address empty; init-kill timer disabled.

Proposed fix: decide the canonical section/key for each setting and make the loader,
the struct defaults, and the shipped `bnetd.toml.in` agree. (E.g. either move these
keys under `[policy]`/`[timing]` in the shipped file, or teach the loader to read them
from `[network]`.)

---

## FINDING 6 — [tracking] trackaddrs key mismatch (HIGH, BUG)

v3 loader (`server_config.cpp:290`) reads `trackserv_addrs`. v3 shipped
(`conf/bnetd.toml.in` `[tracking]`) provides `trackaddrs = "track.pvpgn.pro,..."`.
Key mismatch → shipped tracker list ignored, struct default is empty
(`server_config.hpp:205`, `trackserv_addrs` default ""). `track = 60` (the toggle) is
read correctly. Original compiled `BNETD_TRACK_ADDRS="track.pvpgn.org"`.

Net behavior change: even though tracking is enabled (`track=60`), no tracker addresses
are configured → tracking effectively does nothing.

Proposed fix: rename TOML key to `trackserv_addrs` (or read `trackaddrs` in loader).

---

## FINDING 7 — mail/hashtable/max_friends placed in the wrong TOML section (MED, BUG)

Loader sections:
- `mail_support`, `mail_quota` read from `[account]` (`server_config.cpp:278-279`).
- `hashtable_size`, `max_friends` read from `[policy]` (`server_config.cpp:259-260`).

Shipped TOML places them inverted:
- `[policy]` ships `mail_support = true`, `mail_quota = 5`.
- `[account]` ships `hashtable_size = 61`, `max_friends = 20`.

Result: all four shipped values are in the wrong section and are ignored. Struct
defaults used instead:
- `mail_support=false` (`server_config.hpp:197`) — shipped intends **true**. Behavior
  change: mail subsystem off though conf enables it.
- `mail_quota=10` (`server_config.hpp:198`) — shipped/original intend **5**.
- `hashtable_size=61` — coincidentally matches (`server_config.hpp:183`).
- `max_friends=25` (`server_config.hpp:182`) — shipped/original intend **20**.

Proposed fix: move `mail_*` to `[account]` and `hashtable_size`/`max_friends` to
`[policy]` in the shipped TOML (or change loader sections), and reconcile struct
defaults (see Findings 9, 17).

---

## FINDING 8 — passfail_bantime compiled default differs (MED, BUG)

ORIGINAL (`prefs.cpp:3285`): `conf_set_int(..passfail_bantime, NULL, 300)`.
v3 struct (`server_config.hpp:180`): `passfail_bantime = 0`.
Shipped v3 TOML `[policy]` ships `passfail_bantime = 300` and this key/section *is* read
correctly, so the shipped conf masks the divergence. But for a no-conf / minimal-conf
deployment the v3 fallback is 0 (no ban) vs original 300s ban.

Proposed fix: set struct default `passfail_bantime = 300`.

---

## FINDING 9 — max_friends compiled default 25 vs 20 (MED, UNSURE)

ORIGINAL `MAX_FRIENDS = 20` (`setup_before.h:170`), used by
`prefs.cpp:3148`. Original shipped `max_friends = 20`.
v3 struct `max_friends = 25` (`server_config.hpp:182`). Shipped TOML value (20) is in the
wrong section (Finding 7) and so is ignored → effective default 25.

Net behavior change: friends-list cap 25 instead of 20.

Proposed fix: set struct default to 20 and fix the section (Finding 7). UNSURE only on
whether 25 was an intentional bump.

---

## FINDING 10 — clan_max_members compiled default 100 vs 50 (MED, UNSURE)

See Finding 1 for full chain. ORIGINAL `CLAN_DEFAULT_MAX_MEMBERS=50`. v3 struct=100.
Effective v3 default (with shipped conf, due to key mismatch) is 100.

Proposed fix: struct default 50; fix clan key names.

---

## FINDING 11 — clan_newer_time compiled default 0 vs 168 (MED, UNSURE)

ORIGINAL `CLAN_NEWER_TIME=168` (7 days) (`setup_before.h:163`). v3 struct=0
(`server_config.hpp:287`). Note the original *shipped* conf overrides to 0, and v3
shipped also intends 0 — so the no-conf behavior differs (v3 has no waiting period,
original would have 168h) but configured behavior agrees. Marked UNSURE / lower impact.

Proposed fix: align struct default to 168 to match original compiled fallback.

---

## FINDING 12 — max_connections compiled default 4096 vs 1000 (LOW, UNSURE)

ORIGINAL `BNETD_MAX_SOCKETS=1000` (`setup_before.h:381`), used by `prefs.cpp:3411`.
v3 struct `max_connections=4096` (`server_config.hpp:177`). Shipped TOML 1000 is in the
wrong section (Finding 5) so the effective default is 4096.

Proposed fix: struct default 1000; fix the section placement (Finding 5).

---

## FINDING 13 — irc_latency compiled default 300 vs 180 (LOW, UNSURE)

See Finding 2. Struct default should be 180 to match `BNETD_IRC_LATENCY`.

---

## FINDING 14 — userflush compiled default 3600 vs 1000 (LOW, UNSURE)

ORIGINAL `BNETD_USERFLUSH=1000` (`setup_before.h:208`), used by `prefs.cpp:1185`.
v3 struct `userflush=3600` (`server_config.hpp:146`). Both original *and* v3 shipped
confs set 3600, and the v3 key/section for `userflush` is read correctly, so configured
behavior matches; only the no-conf fallback differs. UNSURE / cosmetic.

---

## FINDING 15 — war3_ladder_update_secs / output_update_secs compiled defaults (LOW, UNSURE)

ORIGINAL compiled both 0 (`prefs.cpp:2836, 2857`). v3 struct 3600 / 300
(`server_config.hpp:273, 280`). Combined with the [ladder]/[status] key mismatch
(Finding 4) the effective v3 defaults are 3600/300 rather than the intended 300/60.

---

## FINDING 16 — sync_on_logoff compiled default true vs false (LOW, UNSURE)

ORIGINAL `prefs.cpp:3453`: default 0 (false). Original shipped `sync_on_logoff = false`.
v3 struct `sync_on_logoff = true` (`server_config.hpp:194`). v3 shipped TOML sets
`sync_on_logoff = false` and the key is read correctly, so configured behavior matches;
no-conf fallback differs (v3 would sync on each logoff). UNSURE / low.

Proposed fix: struct default false to match original.

---

## FINDING 17 — mail_quota compiled default 10 vs 5 (LOW, UNSURE)

ORIGINAL `BNETD_MAIL_QUOTA=5` (`setup_before.h:229`), used by `prefs.cpp:2479`.
v3 struct `mail_quota=10` (`server_config.hpp:198`). Shipped value 5 is in the wrong
section (Finding 7) so the effective default is 10.

Proposed fix: struct default 5; fix section (Finding 7).

---

## FINDING 18 — New per-protocol net.timeouts model has no original analogue (INFO, UNSURE)

v3 adds `[net.timeouts]` with `bnet=300, irc=300, telnet=300, wol=300, bnftp=60,
d2cs=300` (`server_config.hpp:231-238`; loader `server_config.cpp:323-332`). These are
idle-read deadlines that close a connection if no bytes arrive within the window.

The ORIGINAL has no per-protocol idle-read timeout; it relied on `latency` (600s, for
BNCS keepalive accounting), `nullmsg` (120s NULL keepalive), and `irc_latency` (ping
timeout). A 300s hard idle-read deadline on `bnet` could disconnect legitimately-idle
BNCS clients that the original kept connected (the original sends server-side NULLs every
`nullmsg`=120s to keep them alive). Whether the v3 fiber sessions count outbound NULLs as
"activity" determines if this is a real regression. Flagged for verification by the
net-subsystem hunter.

Proposed action: confirm that server-generated keepalive traffic resets the idle-read
deadline, or raise the `bnet` default so idle BNCS clients are not dropped.

---

## FINDING 19 — d2cs idletime compiled default 3600 vs 1800 (LOW, UNSURE)

ORIGINAL `MAX_CLIENT_IDLETIME = 30*60 = 1800` (`src/d2cs/setup.h:106`), used by
d2cs `prefs.cpp:661`. v3 d2cs struct `idletime = 3600`
(`src/infra/config/include/infra/config/d2cs_server_config.hpp:91`). Both ship
`idletime = 3600` in their conf files (`conf/d2cs.conf.in:161`, `conf/d2cs.toml.in:146`),
so configured behavior matches; only the no-conf fallback differs.

---

## KEY DEFAULTS THAT MATCH (coverage / no bug)

Compiled fallbacks AND/OR effective shipped values agree between original and v3:

Ports / addresses:
- bnetd default port **6112** — v3 `network.port=6112`, `bnetdserv_addrs="0.0.0.0:6112"`;
  original `BNETD_SERV_PORT=6112`. ✓ (note: shipped `servaddrs` key is dead, Finding 5,
  but the struct fallback still yields :6112).
- bind address **0.0.0.0** — v3 `bind_addr="0.0.0.0"`; original effectively all-ifaces. ✓
- IRC port **6667** — original `BNETD_IRC_PORT=6667`; v3 documents default 6667 (addr
  commented in both). ✓
- telnet port **23** — original `BNETD_TELNET_PORT=23`; v3 documents 23 (addr commented).✓
- d2cs port **6113** — v3 d2cs `servaddrs="0.0.0.0:6113"`; original `D2CS_SERVER_PORT=6113`,
  `D2CS_SERVER_ADDRS="0.0.0.0"`. ✓
- W3 route port **6200** — both 6200 (but v3 `w3route_addr` shipped value is dead,
  Finding 5; struct fallback is empty). Partial.
- udptest port — original `BNETD_DEF_TEST_PORT=6112`; v3 struct `udptest_port=0`
  (0 = use TCP source port), and shipped both commented. Behaviorally the v3 "0 = use
  source port" is the documented intent. Match in spirit.

Policy / account:
- `new_accounts` default **true/1** — v3 `policy.new_accounts=true`; original
  `prefs.cpp:1505` =1. ✓
- `kick_old_login` **true** — v3=true; original `prefs.cpp:1547`=1. ✓
- `ask_new_channel` **true** — v3=true; original `prefs.cpp:1589`=1. ✓
- `max_accounts` **0** (unlimited) — v3=0; original `prefs.cpp:1526`=0. ✓
- `account_allowed_symbols` **"-_[]"** — v3 `account.account_allowed_symbols="-_[]"`;
  original `PVPGN_DEFAULT_SYMB="-_[]"`. ✓
- `account_force_username` **false** — v3=false; original `prefs.cpp:2983`=0. ✓
- `savebyname` **true** — v3=true; original `prefs.cpp:2542`=1. ✓
- `maxusers_per_channel` **0** (unlimited) — v3=0; original `prefs.cpp:3306`=0. ✓
- `max_conns_per_IP` **0** — v3 struct=0; original `prefs.cpp:3127`=0. ✓ (shipped key
  dead per Finding 5, but fallback agrees).
- `max_concurrent_logins` **0** — v3=0; original `prefs.cpp:2731`=0. ✓
- `passfail_count` **0** — v3=0; original `prefs.cpp:3264`=0. ✓
- `clan_min_invites` **2** — v3=2; original `CLAN_DEFAULT_MIN_INVITES=2`. ✓
- `hashtable_size` **61** — v3 struct=61; original `BNETD_HASHTABLE_SIZE=61`. ✓
  (shipped key in wrong section per Finding 7, but fallback matches.)

Timing / quota:
- `latency` **600** — v3 `timing.latency=600`; original `BNETD_LATENCY=600`. ✓
- `usersync` **300** — v3=300; original `BNETD_USERSYNC=300`. ✓
- `userstep` **100** — v3=100; original `BNETD_USERSTEP=100`. ✓
- `nullmsg` **120** — v3=120; original `BNETD_DEF_NULLMSG=120`. ✓
- `shutdown_delay` **300** / `shutdown_decr` **60** — v3=300/60; original
  `BNETD_SHUTDELAY=300`, `BNETD_SHUTDECR=60`. ✓
- `ipban_check_int` **30** — v3=30; original `prefs.cpp:2710`=30. ✓
- `initkill_timer` **0** compiled — v3 struct=0; original `prefs.cpp:2815`=0. ✓ (shipped
  120 is dead per Finding 5).
- quota: `quota_lines=5`, `quota_time=5`, `quota_wrapline=40`, `quota_maxline=200`,
  `quota_dobae=7` — v3 `messages.*` all match original `BNETD_QUOTA_*`
  (`setup_before.h:222-226`). ✓

Identity / strings:
- `irc_network_name` **"PvPGN"** compiled — v3 struct="PvPGN"; original
  `BNETD_IRC_NETWORK_NAME=PVPGN_SOFTWARE`. ✓ (shipped key dead per Finding 2.)
- `localize_by_country` **true** — both shipped true (`bnetd.conf.in:121`,
  `bnetd.toml.in:125`); note original *compiled* default is false (`prefs.cpp:3494`=0)
  while v3 struct default is true (`server_config.hpp:104,224`) — minor no-conf
  divergence, but the shipped value (true) matches in both, so not flagged as a bug.
- `allowed_clients` **"all"** — v3="all"; original shipped `allowed_clients = all`. ✓
- `report_all_games` **true** — both shipped true. ✓
- `ladder_games` **"none"** — both shipped "none". ✓
- `enable_conn_all` **true** — both shipped true (note original compiled is 0/false
  `prefs.cpp:1693`; both confs ship true; not flagged).

Field-size limits (original `src/common/field_sizes.h`) — these are compile-time
constants in the original; the v3 config layer does not expose them as tunables, so no
divergence to report at the config level:
- `MIN_USERNAME_LEN=2`, `MAX_USERNAME_LEN=16`, `MIN_CHARNAME_LEN=2`,
  `MAX_CHARNAME_LEN=16`, `MAX_USERPASS_LEN=12`, `MAX_CHANNELNAME_LEN=32`,
  `MAX_MESSAGE_LEN=255`, `MAX_IRC_MESSAGE_LEN=512`, `CLAN_NAME_MAX=24`,
  `CLANSHORT_NAME_MAX=4`. (No config key controls these in either codebase — verify
  the v3 protocol/account layer still enforces the same constants; out of scope here.)

d2cs limits that MATCH: `maxchar=8`, `maxgamelist=20`, `shutdown_delay=300`,
`game_maxlevel=255 (0xff)`, `game_maxlifetime=0`, `max_connections=1000`.

---

## NOTE ON ROOT CAUSE

Findings 1-7 share a single root cause: the v3 `bnetd.toml.in` shipped file was authored
with a "natural" TOML key layout (unprefixed keys, settings grouped under the section
that thematically owns them), but `server_config.cpp` reads keys with the legacy
`prefs.cpp` names and groups them under different sections. There is **no schema
validator that checks key names** (`toml_schema_validator.cpp` only checks the top-level
`schema_version` integer), so unknown keys are silently dropped. A key-name/section
audit test comparing every shipped `bnetd.toml.in` key against the loader's expected
keys would catch all of Findings 1-7 at once and is the recommended regression gate.
