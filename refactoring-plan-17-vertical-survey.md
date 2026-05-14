# Vertical Survey & Strangler Order Proposal (16d)

This survey closes out the FINDANONGAME (SID 0x44) vertical and
proposes the next set of verticals to strangler. Source counts are
LOC of `src/bnetd/handle_*.cpp` as a rough indicator of payload
complexity.

## Inventory of legacy entry points

| File                       | LOC      | Vertical / role                                                    |
| -------------------------- | -------- | ------------------------------------------------------------------ |
| `handle_anongame.cpp`      | 49 895   | FINDANONGAME (SID 0x44) - **DONE** (PROFILE + PROFILE_CLAN +       |
|                            |          | TOURNAMENT + GET_ICON + SET_ICON + INFOREPLY all strangled)        |
| `handle_apireg.cpp`        | 25 798   | API registration / external HTTP-style admin                       |
| `handle_bnet.cpp`          | 235 780  | Core BNet packet table (login, chat, channel, friend, clan, etc.)  |
| `handle_bot.cpp`           |  12 741  | Bot client variant                                                 |
| `handle_d2cs.cpp`          |  14 577  | D2CS bridge handler                                                |
| `handle_file.cpp`          |   3 925  | Legacy file transfer (icons, BNI, MPQ snippets)                    |
| `handle_init.cpp`          |   5 190  | Connection init / class detection                                  |
| `handle_irc.cpp`           |  21 174  | IRC dialect (Stealthbot etc.)                                      |
| `handle_irc_common.cpp`    |  11 663  | shared IRC primitives                                              |
| `handle_telnet.cpp`        |  12 774  | Telnet admin shell                                                 |
| `handle_udp.cpp`           |   5 619  | UDP echo + NAT detection - **DONE** (legacy_udp_dispatcher,        |
|                            |          | udp_bridge already strangle this)                                  |
| `handle_wol.cpp`           |  62 177  | Westwood Online (CnC games)                                        |
| `handle_wol_gameres.cpp`   | 123 237  | Westwood game results (CnC ladder)                                 |
| `handle_wserv.cpp`         |   8 909  | Westwood serv aux                                                  |

## Candidate verticals, ordered

### Vertical A - **CHAT / CHANNEL** (recommended next)

Source: subset of `handle_bnet.cpp`, plus
`src/bnetd/{channel,chat,message}.cpp`.

* **Wire surface**: SID_CHATCOMMAND (0x0E), SID_CHATEVENT (0x0F),
  SID_JOINCHANNEL (0x0C), SID_GETCHANNELLIST (0x0B), SID_LEAVECHANNEL
  (0x10).
* **Why first**: smallest leaf, highest QPS in production, and the
  current FINDANONGAME bridge dispatch helper (`dispatch_*_frame`) is
  already a copy-pasted near-duplicate three times - factoring it
  through a chat strangler will force the shared infra extraction.
* **Pure-builder candidates**: command line parsing
  (`handle_command.cpp`), channel rename/MOTD assembly, channel-list
  filtering by clienttag.
* **Estimated batches**: 3-4 (codec extension for 0x0B/0x0C/0x0E/0x0F
  + builder for command parsing + bridge wire-up + channel-list
  golden frames).

### Vertical B - **AUTH / LOGIN** (high value, high complexity)

Source: SID_AUTH_INFO (0x50), SID_AUTH_CHECK (0x51),
SID_LOGONRESPONSE2 (0x3A), SID_LOGONREALMEX (0x3E), SID_AUTH_ACCOUNTLOGON*
(0x53/0x54), plus `bnetd/sql_*` for credential storage.

* **Why interesting**: it's the gating hot-path; everything funnels
  through here.
* **Why deferred**: depends on `bncrypto`, `bnls`, BSHA1, and a real
  account-storage adapter. Worth a dedicated multi-batch plan
  (refactoring-plan-XX-auth.md) before we touch it.

### Vertical C - **REALM / D2CS / D2DBS extension**

* Continue D2DBS protocol vertical (UPDATE_LADDER / CHAR_LOCK to
  finish the codec; codec is already half-done).
* D2CS handlers (`handle_d2cs.cpp`, only 14k LOC, manageable).
* D2GS authentication (already plumbed; codec needs filling out).
* Strangler bridges for `bnetd::d2_*` packet handlers.

### Vertical D - **WOL** (largest single subsystem)

Westwood Online has its own packet shape (telnet-text-based, not
binary) and runs alongside BNet. This is a long tail; not a good
"next" target unless someone is actively running WOL.

### Vertical E - **IRC**

Smaller LOC and well-scoped. Could pair nicely with Vertical A
(channels are the IRC unit too). Defer until after CHAT lands.

### Vertical F - **TELNET ADMIN**

Naturally maps to a v3 admin REST surface (refactoring-plan-13-webui).
Wait until WebUI is being shaped.

## Proposed order

1. **CHAT / CHANNEL** (Vertical A) - leaf, high traffic, forces
   `dispatch_*_frame` extraction.
2. **D2DBS finish + D2CS handlers** (Vertical C, two batches).
3. **AUTH** (Vertical B) - dedicated multi-batch plan first.
4. **IRC** (Vertical E) - reuses CHAT primitives.
5. **WOL** (Vertical D) - separate track, only on demand.
6. **TELNET / API** (Vertical F) - blocked on WebUI plan.

## Cross-cutting cleanup unlocked by Vertical A

* Extract a single `dispatch_bnet_frame(conn, bytes_view)` helper
  from `clan_profile_bridge.cpp` / `profile_bridge.cpp` /
  `tournament_bridge.cpp` / `set_icon_bridge.cpp` /
  `get_icon_bridge.cpp` / `anongame_inforeply_bridge.cpp` (six
  copies right now). Lives in
  `integration/legacy_bnetd/include/integration/legacy_bnetd/dispatch.hpp`.
* Promote the `extern "C" int pvpgn_v3_<op>_try` pattern to a typed
  registry so `handle_bnet.cpp` can iterate strangler hooks instead
  of having seven hand-written `#ifdef` blocks.
