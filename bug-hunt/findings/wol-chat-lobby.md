# Bug Hunt — Westwood Online (WOL) Chat / Lobby

Subsystem: WOL login (APGAR/auth), lobby/channel LIST, JOIN/JOINGAME game model,
GAMEOPT, STARTG, CVERS/VERCHK version check, WOL-specific numeric/reply codes.

- ORIGINAL: `/home/cnupt/work/pvpgn-server/src/bnetd/handle_wol.cpp` (1860 lines),
  `irc.cpp` (shared helpers `irc_send`, `irc_send_cmd`, `irc_send_motd`,
  `irc_send_rpl_namreply`), `src/common/irc_protocol.h` (numeric codes).
- CURRENT v3: `/home/cnupt/work/pvpgn/src/protocol/wol/src/wol_fsm.cpp`,
  `wol_fsm/wol_auth.cpp`, `wol_fsm/wol_chat.cpp`,
  `include/protocol/wol/messages.hpp`, `include/protocol/wol/wol_session_context.hpp`.

## Overall implementation status

The v3 WOL FSM is a **thin skeleton of standard IRC** with WOL command *names*
recognized but almost no WOL-specific *semantics* implemented. The dispatcher
(`wol_fsm.cpp:192-204`) keeps a `wol_known[]` table of 35 WOL commands —
`CVERS, VERCHK, APGAR, SETOPT, SERIAL, GAMEOPT, STARTG, JOINGAME, FINDUSER,
FINDUSEREX, PAGE, ADVERTR, ADVERTC, CHANCHK, GETBUDDY, ADDBUDDY, DELBUDDY, HOST,
INVMSG, INVDEL, USERIP, SQUADINFO, CLANBYNAME, SETCODEPAGE, GETCODEPAGE,
SETLOCALE, GETLOCALE, GETINSIDER, LISTSEARCH, RUNGSEARCH, HIGHSCORE, NAMES,
TOPIC, TIME, KICK, MODE` — and **silently `return core::ok()` for every one of
them** ("// silently accept"). None produce a reply.

Only NICK, USER, PASS, PING, PONG, QUIT, LIST, JOIN, PART, PRIVMSG have bodies,
and those bodies follow generic RFC-1459 IRC, not the WOL dialect that the
Westwood/EA clients require. The `messages.hpp` structs (`Cvers`, `Verchk`,
`Apgar`, `Joingame`, `Gameopt`, `Startg`, etc.) exist as value types but are
**never decoded or used** — the codec/FSM never constructs them; the dispatch
path uses raw `params` strings and ignores the structs entirely.

Net effect: a real WOL client (C&C, Red Alert, Tiberian Sun, RA2, Yuri's
Revenge, Renegade, Emperor, Dune 2000, Nox) cannot log in or do anything useful
against the v3 server — the auth handshake itself diverges (see W-1), and even
if it passed, no game create/join/list/version path produces correct output.

Because of this, most items below are **NOT-IMPLEMENTED** rather than subtle
bugs. I flag the cases that are *actively wrong* (would misbehave if a client
reached them) as BUG, and the structural gaps as NOT-IMPLEMENTED.

---

## W-1 — WOL auth model is wrong: uses BNCS bn_hash/PASS instead of APGAR string-compare

- **Severity:** Critical
- **Classification:** BUG (wrong model) / partially NOT-IMPLEMENTED
- **Original ref:** `handle_wol.cpp:207-281` (`handle_wol_authenticate` /
  `handle_wol_welcome`), `:730-741` (`_handle_apgar_command`), `:368-377`
  (`_handle_pass_command`).

  ```cpp
  // _handle_pass_command: PASS is not used in WOL ... real password sent by APGAR
  static int _handle_pass_command(...) { return 0; }

  // _handle_apgar_command: stores the APGAR token on the connection
  apgar = params[0]; conn_wol_set_apgar(conn, apgar);

  // handle_wol_authenticate: compares the STORED apgar string to the supplied one
  tempapgar = conn_wol_get_apgar(conn);
  temphash  = account_get_wol_apgar(a);
  if (!temphash) { account_set_wol_apgar(a, tempapgar); temphash = account_get_wol_apgar(a); } // auto-create
  if ((tempapgar) && (temphash) && (std::strcmp(temphash, tempapgar) == 0)) { /* OK */ conn_login(...); irc_send_motd(conn); }
  else irc_send(conn, RPL_BAD_LOGIN, ":You have specified an invalid password ...");
  ```

- **v3 ref:** `wol_fsm/wol_auth.cpp:61-77` (`on_pass`), `:79-140`
  (`try_authenticate`). PASS is treated as the real credential; APGAR is in the
  silently-ignored `wol_known[]` table (`wol_fsm.cpp:194-204`), so
  `conn_wol_set_apgar` has no analogue. `try_authenticate` builds an
  `application::auth::LoginRequest` with a **zeroed `BNHash{}`** and relies on a
  generic `IPasswordHasher` to "re-derive" — a BNCS OLS path, not WOL.

- **Divergence:** In WOL the client never sends a usable password via PASS (it
  sends `PASS supersecret` for back-compat only). The real secret arrives via the
  `APGAR <token>` command. The original stores that token verbatim in
  `account_get_wol_apgar` and authenticates by **plain `strcmp`** of stored vs.
  supplied token (auto-registering the token on first login). v3 ignores APGAR
  entirely, authenticates off PASS through the BNCS hashed-login use-case, and
  uses a zeroed hash placeholder. A genuine WOL client will therefore fail auth
  (no APGAR handling) — and even the comment at `wol_auth.cpp:94-98` admits the
  zeroed-hash path is a placeholder "until a WOL-specific hasher is wired in."

- **Proposed fix:** Implement WOL auth as in the original: handle `APGAR` to
  store the token on the session; on USER (or once NICK+USER+APGAR present)
  look up the account's stored WOL apgar, auto-create it if absent, and compare
  by exact string equality; do **not** route WOL through the BNCS hashed
  LoginUser path with a zeroed hash. Treat PASS as a no-op for WOL.

---

## W-2 — Welcome/MOTD sequence diverges: v3 sends 001/002, original sends only MOTD (375/372/376)

- **Severity:** Medium
- **Classification:** BUG (reply-format divergence)
- **Original ref:** `handle_wol.cpp:253-256` — on success the original calls
  `conn_login(...)`, `conn_set_state(conn, conn_state_loggedin)`, then
  `irc_send_motd(conn)`. `irc_send_motd` (`irc.cpp:1193+`) emits `RPL_MOTDSTART`
  (375), `RPL_MOTD` (372) lines, `RPL_ENDOFMOTD` (376). The WOL login path does
  **not** send `RPL_WELCOME` (001) or `RPL_YOURHOST` (002).
- **v3 ref:** `wol_fsm/wol_auth.cpp:127-139` (and the duplicated skeleton block
  `:142-156`): sends `send_numeric(1, ...)` "Welcome to WOL", `send_numeric(2,
  ...)` "Your host is ...", `send_numeric(375, ...)` "- Message of the day -",
  `send_numeric(376, ...)` "End of /MOTD command."
- **Divergence:** v3 injects 001/002 that the original WOL path never sends, uses
  a single hard-coded 375 line of MOTD text (vs. the original reading the motd
  file and emitting 372 body lines), and omits any 372. Some Westwood clients key
  off the exact post-login numeric sequence.
- **Proposed fix:** Match the original: after successful APGAR auth send the MOTD
  block (375 / 372* / 376) without 001/002, sourcing 372 lines from the
  configured motd. If 001/002 are intentionally added, document why; otherwise
  remove.

---

## W-3 — LIST chat-channel reply uses numeric 322 instead of WOL RPL_CHANNEL (327); wrong trailing format

- **Severity:** High
- **Classification:** BUG (WOL numeric divergence)
- **Original ref:** `handle_wol.cpp:552-615` (`_handle_list_command`).
  - Start: `irc_send(conn, RPL_LISTSTART, "Channel :Users Names")` → 321.
  - Each chat channel: built as `"%s %u "` (name, length) + permanent flag
    `"1"`/`"0"`, then **WOLv1 appends `":"`** / **WOLv2 appends `" 388"`**, sent
    via `irc_send(conn, RPL_CHANNEL, temp)` where `RPL_CHANNEL == 327`
    (`irc_protocol.h:334`).
  - Games are listed separately via `RPL_GAME_CHANNEL == 326`
    (`append_game_info`, `:547`).
  - End: `irc_send(conn, RPL_LISTEND, ":End of LIST command")` → 323.
- **v3 ref:** `wol_fsm/wol_chat.cpp:43-91` (`on_list`). Emits 321 with text
  `"Channel :Users  Name"`, then for each channel a **`322`** line
  `<name> <count> :` (`:64-73`), then 323 `"End of /LIST"`.
- **Divergence:** (a) Wrong numeric — WOL chat channels use **327**, not the
  standard IRC **322**. (b) No permanent-vs-user flag column. (c) Missing the
  WOLv1 `":"` / WOLv2 `" 388"` trailing token that distinguishes protocol
  versions. (d) Games are not listed at all (should be separate **326**
  entries with the `currentusers maxplayers gameType tournament gameExtension
  longIP LOCK::topic` layout, see `append_game_info` `:501-547`). (e) 321/323
  trailing text differs. The original also gates by `tag_check_wolv1` channel
  name length and skips game-channels from the chat list.
- **Proposed fix:** Implement WOL LIST faithfully: 321 start, 327 per chat
  channel with the flag column + version-specific trailing token, 326 per open
  game with the full game-info layout, 323 end. Add the WOLv1/WOLv2 branching.

---

## W-4 — VERCHK / CVERS version check not implemented (no 379 reply, clienttag never set)

- **Severity:** High
- **Classification:** NOT-IMPLEMENTED
- **Original ref:**
  - `_handle_cvers_command` (`handle_wol.cpp:673-696`): `CVERS [oldver] [SKU]` →
    sets clienttag via `tag_sku_to_uint(atoi(params[1]))` (the SKU is the *first*
    command the client sends and is how the server learns which game it is).
  - `_handle_verchk_command` (`:698-728`): `VERCHK [SKU] [version]` → sets
    clienttag from `params[0]`, then replies
    `irc_send(conn, RPL_VERCHK_NONREQ, ":none none none 1 <SKU> NONREQ")` where
    `RPL_VERCHK_NONREQ == 379`. Without this reply the client will not proceed
    past the patch-check stage.
- **v3 ref:** Both `CVERS` and `VERCHK` are in the silently-accepted
  `wol_known[]` table (`wol_fsm.cpp:194-200`); no reply, no clienttag set. The
  `Cvers`/`Verchk` structs in `messages.hpp:200-214` are never used.
- **Divergence:** No 379 verchk reply is ever sent and the client product/tag is
  never identified, so per-game behavior (channel typing, game lists, ladders)
  cannot work. This is a hard blocker for any real client.
- **Proposed fix:** Implement CVERS (parse SKU → set client tag) and VERCHK
  (parse SKU+version → set tag, reply 379 `:none none none 1 <SKU> NONREQ`).

---

## W-5 — JOINGAME (create/join game over WOL channel-as-game model) not implemented

- **Severity:** High
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `_handle_joingame_command` (`handle_wol.cpp:908-1128`). Two
  modes selected by `numparams`:
  - **Join** (numparams 2 or 3): looks up the game via
    `gamelist_find_game_available`, checks full/banned/password, joins the
    game's channel, and emits a `message_wol_joingame` ack with the WOLv1 vs
    WOLv2 layout
    `<min> <max> <gameType> 1 [1|clanID] [clanID|longIP] <tournament> :<#chan>`
    (`:1019-1034`), plus topic + namreply. Note the RA1 v3.03e bug forward to
    `_handle_join_command` (`:944-953`).
  - **Create** (numparams >= 7): builds game options string, creates the channel
    + game, sets maxplayers/min/gameType/gameExtension/password, sends the
    joingame ack (`:1054-1124`).
- **v3 ref:** `JOINGAME` is silently accepted (`wol_fsm.cpp:195`). The `Joingame`
  struct (`messages.hpp:259-267`) even models the **wrong** fields —
  `game_name / host_ip / host_port` — which is a plain-IRC mental model, not the
  WOL `#chan min max channelType ... tournament [ext] [pass]` format.
- **Divergence:** Entire game create/join path absent. The `messages.hpp`
  `Joingame` field layout does not match the WOL wire format at all.
- **Proposed fix:** Implement both JOINGAME modes against the v3 game/channel
  domain; redefine the `Joingame` struct fields to the real WOL layout
  (min/max players, channelType, tournament flag, gameExtension, optional
  password) and reproduce the WOLv1/WOLv2 ack format.

---

## W-6 — GAMEOPT / STARTG game-option and game-start relays not implemented

- **Severity:** High
- **Classification:** NOT-IMPLEMENTED
- **Original ref:**
  - `_handle_gameopt_command` (`handle_wol.cpp:1130-1182`): relays the opaque
    game-options text to the channel (`message_type_gameopt_talk`) or to a user
    (`message_type_gameopt_whisper`); output
    `user!WWOL@hostname GAMEOPT <target> :<gameOptions>`.
  - `_handle_startg_command` (`:1264-1344`): sets game status started and sends
    `message_wol_start_game` with player/IP list; WOLv1 vs WOLv2 layouts
    (`:1278-1283`) — e.g. WOLv2
    `:user1!WWOL@hostname STARTG u :user1 ip user2 ip :gameNumber time_t`.
- **v3 ref:** Both silently accepted (`wol_fsm.cpp:195-196`). The `Gameopt`
  (`messages.hpp:244-248`) and `Startg` (`:252-257`) structs are unused.
- **Divergence:** No game-option propagation and no game-start signaling — a
  hosted game can never actually launch through v3.
- **Proposed fix:** Implement GAMEOPT (channel/whisper relay of opaque options)
  and STARTG (mark started, emit per-player STARTG with IP list, WOLv1/WOLv2
  branch).

---

## W-7 — QUIT reply diverges: v3 sends generic "ERROR :Closing Link"; original sends RPL_QUIT (607) ":goodbye"

- **Severity:** Low
- **Classification:** BUG (reply divergence)
- **Original ref:** `_handle_quit_command` (`handle_wol.cpp:617-627`):
  `conn_quit_channel(...)` then `irc_send(conn, RPL_QUIT, ":goodbye")` where
  `RPL_QUIT == 607` (`irc_protocol.h:360`), then `conn_state_destroy`.
- **v3 ref:** `wol_fsm/wol_chat.cpp:36-41` (`on_quit`): `send_raw("ERROR :Closing
  Link")` then close.
- **Divergence:** Original emits a numeric **607** `:goodbye`; v3 emits a raw
  `ERROR` line. Different wire output.
- **Proposed fix:** Send `:server 607 <nick> :goodbye` (or match the exact
  original) on QUIT.

---

## W-8 — JOIN reply format diverges from WOL irc_send_rpl_namreply; names are numeric account IDs; spurious 332

- **Severity:** Medium
- **Classification:** BUG (reply-format divergence)
- **Original ref:** `_handle_join_command` (in `irc.cpp`) and
  `irc_send_rpl_namreply` (`irc.cpp:~1040`) build the 353 names line from
  channel members' **chat names** with proper `(ohv)@%+` user-mode prefixes and
  `irc_convert_channel` channel naming. WOL JOIN does not send a 332 TOPIC unless
  a topic exists (TOPIC handling is separate).
- **v3 ref:** `wol_fsm/wol_chat.cpp:93-190` (`on_join`).
  - Use-case path (`:135-151`): builds 353 by iterating
    `channel.member_ids()` and appending **`std::to_string(mid.value())`** —
    i.e. the 353 name list is numeric account IDs, not nicknames (comment at
    `:148` admits "use numeric string as placeholder"). A client will display
    garbage member names.
  - Stub path (`:184-186`): sends `send_numeric(332, channel_, "")` — an empty
    TOPIC unconditionally, which the original WOL JOIN does not do.
  - JOIN echo prefix is hard-coded `@Battle.net` (`:131`, `:166`) rather than the
    WOL `@hostname`/server form.
- **Divergence:** 353 member names are account-id integers (placeholder), an
  empty 332 is injected in stub mode, and the JOIN-echo host is hard-coded.
- **Proposed fix:** Resolve member account IDs to chat names for the 353 reply,
  drop the unconditional empty 332, and use the server hostname in the JOIN echo
  prefix consistent with the original.

---

## W-9 — PONG reply format diverges (missing server name middle token)

- **Severity:** Low
- **Classification:** BUG (reply-format divergence)
- **Original ref:** `irc_send_pong` (`irc.cpp:187-190`):
  `:%s PONG %s :%s` → `:<server> PONG <server> :<token>` (server name appears
  twice — as prefix and as the PONG target).
- **v3 ref:** `wol_fsm/wol_chat.cpp:26-34` (`on_ping`): replies `PONG :<token>`
  with **no** server prefix and **no** server-name middle parameter.
- **Divergence:** v3 omits the `:<server> ... <server>` framing the original
  uses. Strict clients may reject or mis-parse.
- **Proposed fix:** Emit `:<server_name> PONG <server_name> :<token>`.

---

## W-10 — PRIVMSG: no `matchbot` (anongame) routing, no CTCP ACTION/emote, no `/command` handling, wrong whisper error target

- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED (anongame, CTCP, commands) + minor BUG
- **Original ref:** `_handle_privmsg_command` (`handle_wol.cpp:379-446`):
  - Routes messages to `"matchbot"` into `anongame_wol_privmsg`
    (`anongame_wol.cpp`) — the WOL automatch system.
  - Detects `\001ACTION ...\001` CTCP → `message_type_emote`.
  - Lines starting with `/` → `handle_command` (server commands).
  - Whisper to offline user → `irc_send(conn, ERR_NOSUCHNICK, ":No such user")`.
- **v3 ref:** `wol_fsm/wol_chat.cpp:213-266` (`on_privmsg`): channel post via
  `post_message_` use-case or silent stub; no matchbot/anongame, no CTCP ACTION,
  no `/`-command dispatch. Whisper-to-user always returns 401
  `<target> :No such nick` even when the target exists (private delivery is
  unimplemented — `:263-265`).
- **Divergence:** Anongame automatch (the whole `matchbot` path), emotes, and
  in-channel slash-commands are absent; user-to-user whisper never delivers.
- **Proposed fix:** Add matchbot→anongame routing, CTCP ACTION emote handling,
  `/`-command dispatch, and real whisper delivery (401 only when the target is
  genuinely offline).

---

## W-11 — All other WOL commands stubbed: SETOPT, SERIAL, SETCODEPAGE/GETCODEPAGE, SETLOCALE/GETLOCALE, GETINSIDER, FINDUSER(EX), PAGE, ADVERTR, CHANCHK, GETBUDDY/ADDBUDDY/DELBUDDY, HOST, INVMSG, USERIP, SQUADINFO, CLANBYNAME, NAMES, TOPIC, TIME, KICK, MODE, LISTSEARCH/RUNGSEARCH/HIGHSCORE

- **Severity:** Medium (collectively High for feature parity)
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** Each has a real handler in `handle_wol.cpp` producing a
  specific WOL numeric/relay, e.g.:
  - `SETOPT` (`:783-810`) toggles find/page flags; `SERIAL` (`:743-747`) ignored
    (this one matches v3's no-op).
  - `SETCODEPAGE`/`GETCODEPAGE` (`:812-850`) → 329 / 328 with backtick lists.
  - `SETLOCALE`/`GETLOCALE` (`:852-889`) → 310 / 309.
  - `GETINSIDER` (`:891-906`) → 399 `<nick>`0`.
  - `FINDUSER` (`:1184-1206`) → 388 `0 :<channel>` / `1 :`;
    `FINDUSEREX` → 398 `0 :<channel>,0` / `1 :`.
  - `PAGE` (`:1232-1262`) → 389 `0 :` / `1 :`, with BATTLECLAN broadcast.
  - `ADVERTR` (`:1346-1367`) → `message_wol_advertr` `5 <channel>`.
  - `CHANCHK` (`:1375-1407`) → `message_wol_chanchk` or 403.
  - `GETBUDDY`/`ADDBUDDY`/`DELBUDDY` (`:1409-1525`) → 333 / 334 / 335 backtick
    buddy lists.
  - `HOST` (`:1527-1547`), `INVMSG` (`:1549-1584`), `USERIP` (`:1592-1615`) →
    `message_wol_*` relays.
  - `SQUADINFO`/`CLANBYNAME` (`:749-781`) → 358 RPL_BATTLECLAN clan-info line or
    `ERR_IDNOEXIST`.
  - `LISTSEARCH`/`RUNGSEARCH`/`HIGHSCORE` (`:1669-1856`) → ladder server raw
    replies (`_ladder_send`).
- **v3 ref:** All in `wol_known[]` (`wol_fsm.cpp:194-201`), each
  `return core::ok()` with no reply.
- **Divergence:** None of these features exist in v3. Many are required for the
  in-game UI (buddy list, find/page, clan/squad info, codepage/locale, ladder).
- **Proposed fix:** Implement per-command against the v3 domain, matching the
  WOL numeric codes and backtick-delimited payload formats above.

---

## W-12 — `messages.hpp` struct field layouts model standard IRC, not WOL wire formats (dead + misleading)

- **Severity:** Low (currently dead code) / Medium (will mislead implementers)
- **Classification:** BUG (latent — wrong field model)
- **v3 ref:** `messages.hpp`:
  - `Joingame { game_name; host_ip; host_port; }` (`:259-267`) — WOL JOINGAME has
    no host_ip/host_port; it carries min/max players, channelType, tournament
    flag, gameExtension, optional password (see W-5).
  - `Apgar { password_hash; flags; }` (`:216-223`) — APGAR is a single token
    argument in the original (`params[0]`), not `hash + flags`.
  - `Page { nickname; message; }` (`:277-284`) — original PAGE target can be `0`
    meaning "my battleclan" (broadcast), not a plain nick.
- **Divergence:** These structs are never decoded today (FSM uses raw param
  strings), but their field layouts encode an incorrect model of the protocol
  and will steer a future implementer wrong.
- **Proposed fix:** When wiring the codec, redefine these structs to the actual
  WOL field layouts; until then add comments flagging them as provisional.

---

## W-11 progress: FINDUSER / FINDUSEREX — DONE (wave 35)

`on_finduser` (FINDUSER 388 / FINDUSEREX 398): resolves the target nick ->
account (account_reader) and reports presence — "0 :<channel>" when the account
has a live session (WOL `findme` defaults on, so online == findable; channel
resolved via channel_reader), "1 :" otherwise; FINDUSEREX appends ",0". Reply is
built raw (`:server <code> <nick> <payload>`, payload verbatim) to match the
original `irc_send_cmd` framing rather than send_numeric (which would inject an
extra ':'). `diff_wol_finduser.py`: online user -> 0, unknown -> 1, FINDUSEREX ->
0, all match the oracle. Remaining W-11 stubs: SETOPT, PAGE, GETBUDDY/ADDBUDDY/
DELBUDDY (maps to the existing friend use-cases), codepage/locale, GETINSIDER,
SQUADINFO/CLANBYNAME, CHANCHK, HOST/INVMSG/USERIP, ladder LISTSEARCH/etc.

## What MATCHES (or is acceptably close)

- Line framing: v3 `process_lines` handles `\r\n` and bare `\n`, with a 512-byte
  RFC-1459 line cap and an overflow guard (`wol_fsm.cpp:38-167`) — reasonable and
  close to original IRC framing.
- Numeric formatting: `send_numeric` produces `:<server> NNN <target> :<text>`
  (`wol_fsm.cpp:215-235`), matching the original `irc_send_cmd`/`irc_send`
  `:%s %s %s %s` shape (`irc.cpp:79-115`) — though the original always uses the
  *loggeduser* as the target token, whereas v3 passes an explicit `target`
  (usually `nick_`); equivalent in the common case.
- `SERIAL` as a no-op matches the original (`handle_wol.cpp:743-747`).
- Command dispatch is case-insensitive in both (original `strcasecmp`; v3
  upper-cases the command in `parse_line`, `wol_fsm.cpp:42-75`).
- Unknown-command handling: v3 returns 421 ERR_UNKNOWNCOMMAND for truly unknown
  commands (`wol_fsm.cpp:206-208`), a sane default the original effectively
  shares (returns -1 → handled upstream).

## Notes / methodology

- Read-only review; no source edited, no build run.
- The `src/integration/wol` directory contains only `wol_session.cpp` /
  `wol_session_factory.hpp` (transport glue) and was not the source of protocol
  bugs; the protocol logic lives entirely in `src/protocol/wol`.
- `anongame_wol.cpp` (original, 615 lines) implements the WOL automatch
  (`matchbot`) flow; v3 has **no** counterpart at all (see W-10) — entire
  automatch subsystem NOT-IMPLEMENTED. (The gameres path was out of scope per the
  brief; automatch chat-side entry is in scope and is absent.)

---

## F-W22 — Full supported-client mock matrix; differentially confirms v3 cannot complete the WOL login handshake

**Severity:** the mock catalog + WOL mock are tooling; the v3 WOL-auth gap is
HIGH (whole Westwood client family cannot authenticate) — confirmed, not fixed.
**Classification:** tooling added; v3 gap CONFIRMED (matches the skeleton review
above).

Added mock clients covering PvPGN's entire published supported-client matrix and
a driver that boots BOTH servers and logs each in with the matching mock:

- `tests/diff/clients.py` — authoritative catalog of every supported product +
  version (21 products / 149 versions / 19 distinct protocol paths), grouped into
  three login families (ols / nls / wol) with tag, arch, WOL SKU, and a
  `login()` dispatcher.
- `tests/diff/wol_client.py` — a faithful Westwood Online mock: connects to the
  WOL listener and drives `CVERS <sku>` → `VERCHK` → `APGAR <pwtoken>` → `NICK`
  → `USER` → welcome/MOTD, exactly as `handle_wol.cpp` expects. The APGAR token
  is a deterministic password-derived string (the original stores/compares it
  opaquely and auto-creates the account on first login, so this is faithful).
- `tests/diff/original_server.py` — now enables `wolv1addrs`/`wolv2addrs` on
  derived test ports (off by default in pvpgn) so the oracle accepts WOL.
- `tests/diff/diff_all_clients.py` — drives one representative per protocol path
  against the oracle (and v3 for ols/nls).

**Result (diff_all_clients.py):** all 19 paths log in against the ORACLE (19/19);
OLS + NLS match v3 (8/8); every WOL path FAILS against v3 (0/11) — confirming the
skeleton review above with a running differential. v3's `wol_auth.cpp`
authenticates via standard IRC `NICK`/`USER`/`PASS` and never processes `APGAR`
(it `return core::ok()`s for CVERS/VERCHK/APGAR), so the Westwood welcome/MOTD is
never sent and no WOL client can log in. The OLS/NLS mocks are now exercised for
every supported tag (STAR/SEXP/W2BN/DRTL/D2DV/D2XP and WAR3/W3XP) in one run.

**To fix the v3 WOL gap (future, larger feature):** implement the WOL auth path
in `wol_auth.cpp` — store the APGAR token, map CVERS SKU → clienttag, reply to
VERCHK (379 NONREQ), and on USER (with NICK+APGAR present) auto-create/verify the
account and send the welcome/MOTD — mirroring `handle_wol_authenticate`. Then the
WOL rows in `diff_all_clients.py` flip to match and a `diff_wol_login.py` can
assert it the way `diff_w3_login.py` does for SRP-3.

---

## F-W23 — WOL auth gap (F-W22) RESOLVED: native CVERS/APGAR login implemented in v3

The WOL-auth gap from F-W22 is now fixed. v3's `wol_auth.cpp` authenticates the
Westwood way: a new `WolAuthDeps`-injected `WolFsm` constructor wires
`CreateAccount` + a new `IWolCredentialStore` (with `InMemoryWolCredentialStore`,
mirroring the SRP-3 store), and new `on_cvers` / `on_verchk` / `on_apgar` handlers
plus `try_wol_authenticate()` reproduce `handle_wol_authenticate`: VERCHK replies
379 NONREQ; APGAR stores the opaque token; USER (with NICK+APGAR) auto-creates the
account on first login and verbatim-compares the stored token thereafter, then
sends the welcome/MOTD; a wrong token yields 378 RPL_BAD_LOGIN. Wired into the
live WOL listener in main.cpp via a run-loop-scoped `InMemoryWolCredentialStore`.

Also fixed a latent robustness limitation: the WOL/IRC listener ports were
hardcoded (4000/6667), so two bnetd instances could never coexist. Added
`--wol-port` / `--irc-port` CLI flags; the diff harness now gives each instance
unique derived ports.

Verified: `diff_all_clients.py` now reports **19/19 oracle logins and 19/19 v3
matches** across OLS + NLS + WOL (was 0/11 for WOL). New differential
`diff_wol_login.py` matches the oracle on first-login auto-create, correct
re-login, and wrong-password rejection. Unit guard:
`tests/unit/protocol/wol/wol_fsm_native_auth_test.cpp` (6 cases).

Still skeleton (separate from auth): the post-login WOL lobby/game commands
(LIST/JOIN game model, GAMEOPT, STARTG, the automatch `matchbot`) remain
no-ops — see the body of this file. Auth (the entry gate for the whole Westwood
family) is the part that is now faithful end to end.

---

## F-W29 — WOL post-login lobby LIST/JOIN over the shared channel repo (DONE)

`on_list` now emits the WOL `327 RPL_CHANNEL "<name> <count> <official> 388"`
(WOLv2) form instead of the standard IRC 322, and `on_join` relays through the
JoinChannel use-case over the shared channel repository. `diff_wol_lobby.py`:
a joined channel appears in LIST on both the oracle and v3. (W-3 partially
addressed for chat channels; game-channel 326 entries still pending — see W-5.)

---

## F-W31 — WOL channel chat is delivered across sessions (DONE) — the routing foundation for the game lobby

**The gap (was W-10, partial):** `on_privmsg` posted a channel message via the
PostMessage use-case but then **dropped the returned recipient SessionId list on
the floor** — WolFsm had no message router and WOL sessions were never registered
with the cross-session `MessageRouterImpl`. Two WOL clients in the same channel
never heard each other. This is also the exact mechanism GAMEOPT/STARTG need to
broadcast, so it blocked the whole game lobby.

**Fix (commit c9c7133, wave 31):**
- `make_wol_session` assigns each WOL connection a real `SessionId`
  (`next_session_id()`) and registers its egress with the shared
  `MessageRouterImpl`, unregistering on close. New `WolFsm::set_routing()`.
- `on_privmsg` encodes the standard IRC line once —
  `:<nick>!<nick>@Battle.net PRIVMSG <#chan> :<text>` — and routes it to every
  recipient SessionId from PostMessage. No self-echo (matches the original).
- Session identity now lines up across the single shared session registry: WOL
  auth `attach`es account→session, PostMessage resolves members to those
  SessionIds, the router delivers to the registered egress.

**Verified:** `tests/diff/diff_wol_chat.py` — two WOL clients A,B join `#wolchat`;
A PRIVMSGs, B receives the same text on **both** the oracle and v3. New
`wol_client.py` helpers `wol_privmsg` / `wol_read_privmsg`. Unit suite (3150)
green; the null-router stub path (unit tests) is unchanged.

### Next increment plan — GAMEOPT / STARTG / JOINGAME (the remaining game lobby)

Unblocked by F-W31. Oracle ref: `handle_wol.cpp` `_handle_gameopt_command` (1130),
`_handle_startg_command` (1264), `_handle_joingame_command` (908).

1. **GAMEOPT channel-talk — DONE (wave 33).** `on_gameopt` now relays the opaque
   options text: channel mode (`#...`) broadcasts
   `:<nick>!<nick>@Battle.net GAMEOPT <#chan> :<text>` to the current channel's
   members (resolved via a `channel_reader` threaded into WolFsm +
   `auth_.session_registry`, excluding self — mirrors
   `channel_message_send(message_type_gameopt_talk)`); whisper mode resolves
   nick→account→session via `auth_.account_reader` (401 if offline). Shared
   `route_irc_line()` helper (on_privmsg now uses it too). `diff_wol_gameopt.py`:
   B receives A's options on both oracle and v3. `make_wol_session`/`set_routing`
   now carry the channel reader (`&channel_repo`).
2. **JOINGAME create/join — DONE (wave 34).** New WOL game-channel registry
   (`application::game::IWolGameStore` + `InMemoryWolGameStore`, run-loop scoped,
   shared across WOL sessions) holds the WOL-specific metadata (min/max players,
   game type, tournament, gameExtension, password, host, channel id, players) —
   kept apart from the BNCS IGameRepository. `on_joingame`: CREATE (>=7 params)
   registers the game + creates the backing channel (JoinChannel) + acks the host
   `<min> <max> <type> <p4> 0 <tourn> :#name` (WOLv1); JOIN (2-3 params) finds the
   game (478 if closed), checks full (471) / password (475), joins the channel,
   and acks **every channel member** `<min> <max> <type> 1 1 <tourn> :#chan` via
   the router (so the host learns a player joined). `diff_wol_joingame.py`: A
   creates #wolgame, B joins, B's ack `2 8 1 1 1 0` matches the oracle.
3. **STARTG** (game model now exists, but BLOCKED on peer-IP tracking):
   `game_set_status(started)` + per-player STARTG with the IP list — WOLv2
   `:<owner>!WWOL@host STARTG u :user1 ip user2 ip :gameNumber time_t`, WOLv1 the
   owner-IP form. The payload's reason for existing is the per-player IPs (P2P
   game setup), which v3 cannot currently produce: there is no SessionId/account
   -> peer-IP map (TcpSession knows its own remote_endpoint, but WolFsm can't
   reach another session's IP). Faithfully reproducing STARTG therefore needs a
   peer-IP registry first — infra disproportionate to the single verb — and the
   gameNumber + time_t fields make a byte-exact differential impossible anyway
   (they differ per server/run). Deferred until peer-IP tracking exists; the
   control flow (mark started + route a STARTG line to named players) is trivial
   on top of the wave-31 router once the IPs are available.

GOTCHA carried forward: running-server diffs must be launched with the Bash tool
`run_in_background:true` (a foreground `timeout|tee` pipeline that boots the
oracle is killed by the harness with exit 144; foreground `sleep` is blocked).
Leftover sanitizer/dev bnetd procs squat ports — `pkill -9 -f build/<variant>/...`
between runs.
