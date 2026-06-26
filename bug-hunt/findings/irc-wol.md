# Bug Hunt: IRC + WOL Protocols — Original vs v3

Scope: IRC numeric reply codes, command keywords/parsing (trailing `:` param,
space splitting), WOL verbs & reply formats, CRLF/line-length, prefix format.

Original (read-only): `/home/cnupt/work/pvpgn-server/src/bnetd/`
- `irc.cpp`, `handle_irc.cpp`, `handle_irc_common.cpp`, `handle_wol.cpp`,
  `common/irc_protocol.h` (numeric defines)

v3 (read-only): `/home/cnupt/work/pvpgn/src/protocol/`
- IRC: `irc/src/codec.cpp`, `irc/src/fsm.cpp`, `irc/src/fsm/irc_registration.cpp`,
  `irc/src/fsm/irc_commands.cpp`
- WOL: `wol/src/wol_fsm.cpp`, `wol/src/wol_fsm/wol_auth.cpp`,
  `wol/src/wol_fsm/wol_chat.cpp`

NOTE on intent: The v3 IRC/WOL FSMs are explicitly skeletal ("stub", "skeleton",
"not yet implemented" comments throughout). Many gaps are unimplemented features
(WHOIS only knows self, no NICKSERV, no real channel membership). Those are flagged
INTENTIONAL/incomplete. The findings below focus on cases where v3 *does* emit a
reply and the wire format / numeric / parsing diverges from the original in a way
that breaks a real client expecting parity.

---

## FINDING 1 — `make_numeric` drops the implicit `nick` slot; uses caller's target as nick

Severity: HIGH
Classification: BUG

Original: `irc.cpp:79-115` (`irc_send_cmd`) + `irc.cpp:117-131` (`irc_send`).
Every numeric reply is wired as:
```
:<ircname> <command> <nick> <params>\r\n      (irc.cpp:104)
```
i.e. the server ALWAYS inserts the logged-in user's nick as the first argument
after the numeric code, and the caller-supplied `params` string follows. So
`irc_send(c, RPL_ENDOFNAMES, "#chan :End of NAMES list")` produces:
```
:server 366 <nick> #chan :End of NAMES list
```

v3: `fsm.cpp:32-45` (`make_numeric`) builds:
```
m.params = { target, text }      → :server NNN <target> <text>
```
and `send_numeric(code, target, text)` is called throughout with `target` being
sometimes the nick and sometimes a channel.

Divergence: In v3 the "first argument after the numeric" is whatever the handler
passes as `target`. Multiple handlers pass a CHANNEL instead of the nick:
- `irc_commands.cpp:152` `send_numeric(366, channel_, ...)` → `366 #chan End of...`
  Missing the nick argument entirely. Original 366 line is
  `<nick> <channel> :End of /NAMES list`.
- `irc_commands.cpp:461` `send_numeric(366, target_chan, ...)` — same.
- `irc_commands.cpp:399` MODE `send_numeric(324, nick_, target + " +")` — here the
  text field carries `#chan +`, so wire is `324 <nick> #chan +` — OK-ish, but the
  mode string is the trailing param with a leading space rather than separate args.

For RFC numerics the canonical layout is `:server NNN <nick> <args...> :<trailer>`.
The original guarantees the `<nick>` slot via `irc_send_cmd`. v3's `make_numeric`
has no such guarantee — it only emits two params. Any numeric whose original
`params` string did NOT itself begin with the nick will now be missing the nick
target, and any numeric that needs >1 arg before the trailer collapses them into a
single space-joined param.

Real-client impact: clients parse `RPL_ENDOFNAMES` etc. positionally; a missing
first-arg nick shifts every field left and breaks NAMES/WHO/LIST end detection.

Proposed fix: `make_numeric` (and `WolFsm::send_numeric`) must always emit
`<nick>` as the first param, then the handler-supplied args, then the trailer —
mirroring `irc_send_cmd`'s `:server NNN nick <params>`. Handlers should pass the
post-nick argument list, not overload `target` as the nick.

---

## FINDING 2 — RPL_NAMREPLY (353) channel-status prefix dropped

Severity: MEDIUM
Classification: BUG (parity) / partially INTENTIONAL (membership unimplemented)

Original: `irc.cpp:980-981` builds the 353 trailer as:
```
%c %s :<members>     where %c = '=' (permanent/public) or '*' (private)
```
→ `:server 353 <nick> = #chan :user1 user2` — note the `=`/`*`/`@` channel-status
char is a SEPARATE param before the channel name, per RFC2812.

v3 IRC: `fsm.cpp:57-78` (`send_names_reply`) DOES emit `=` then channel then
members — this MATCHES the original structure. Good.

v3 WOL: `wol_chat.cpp:135-151` and `:171-182` build `353 <nick> = <channel> :...`
which also matches.

Divergence: only the *membership content* differs — v3 uses numeric account-id
strings as placeholders (`irc_commands.cpp:146`, `wol_chat.cpp:149`) instead of
chatnames, and omits the per-user flag prefixes (`@`/`%`/`+`) that the original
computes (`irc.cpp:997-1002`). This is unimplemented membership, flagged for
follow-up, not a format bug per se.

Proposed fix: when membership lookup is wired, prepend user flags and use
chatnames, matching `irc_send_rpl_namreply_internal`.

---

## FINDING 3 — WOL QUIT does not send RPL_QUIT (607)

Severity: MEDIUM
Classification: BUG

Original: `handle_wol.cpp:617-627` (`_handle_quit_command`) sends:
```
irc_send(conn, RPL_QUIT, ":goodbye");    → :server 607 <nick> :goodbye
```
then destroys the connection.

v3 WOL: `wol_chat.cpp:36-41` (`on_quit`) sends:
```
ERROR :Closing Link
```
i.e. a raw `ERROR` line, not the numeric 607 the Westwood client expects.

Divergence: WOL clients keyed on `607` for clean disconnect; the original never
sends a bare `ERROR`. The IRC-side original QUIT (`handle_irc.cpp:532-537`) sends
nothing back at all (just destroys), so v3 IRC's `on_quit` (`irc_commands.cpp:57`)
sending nothing is closer; but v3 WOL inventing `ERROR :Closing Link` diverges
from both.

Proposed fix: WOL `on_quit` should `send_numeric(607, nick_, "goodbye")` (trailer
`:goodbye`) before close, dropping the `ERROR` line.

---

## FINDING 4 — Numeric 401 trailer text differs ("No such nick" vs original)

Severity: LOW
Classification: BUG (minor text/parity)

Original PRIVMSG whisper miss: `handle_irc.cpp:373`
```
irc_send(conn, ERR_NOSUCHNICK, ":No such user");        → 401 <nick> :No such user
```
Original WHOIS miss: `handle_irc.cpp:605`
```
irc_send(conn, ERR_NOSUCHNICK, ":No such nick/channel");
```

v3: `irc_commands.cpp:271` PRIVMSG → `target + " :No such nick"` and
`:329` WHOIS → `target + " :No such nick/channel"`.

Divergence: v3 prepends the target nick (RFC-correct: `401 <nick> <target> :...`)
which is arguably *more* correct than the original, but the trailer wording for
PRIVMSG differs ("No such nick" vs original "No such user"). Low impact (text
only) but a parity deviation. Note WOL original PRIVMSG miss also uses
`":No such user"` (`handle_wol.cpp:435`) while v3 WOL `wol_chat.cpp:265` uses
`":No such nick"`.

Proposed fix: align trailer text if strict parity desired; otherwise document the
intentional RFC alignment.

---

## FINDING 5 — IRC PRIVMSG with no text returns 411 instead of 461

Severity: MEDIUM
Classification: BUG

Original: `handle_irc.cpp:381-382` — when `numparams < 1` OR no text:
```
irc_send(conn, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");  → 461
```
The original treats "missing recipient and/or text" uniformly as **461
ERR_NEEDMOREPARAMS**.

v3 IRC: `irc_commands.cpp:234-237` — when `m.params.size() < 2`:
```
send_numeric(411, nick_, "PRIVMSG :No recipient or text");   → 411
```
v3 WOL: `wol_chat.cpp:221-222` returns 411, and `:231-233` returns 412 for empty
text.

Divergence: wrong numeric. Original always sends 461 for the underflow case;
v3 sends 411 (ERR_NORECIPIENT) / 412 (ERR_NOTEXTTOSEND). A client scripted
against PvPGN's 461 behavior will mis-handle. (411/412 are arguably more RFC-pure,
but this is a parity divergence.)

Proposed fix: return 461 with `"PRIVMSG :Not enough parameters"` to match, or
confirm the 411/412 split is an intentional RFC upgrade.

---

## FINDING 6 — IRC PONG handler missing (no PONG keyword in v3 IRC dispatch)

Severity: MEDIUM
Classification: BUG

Original: `handle_irc.cpp:85` registers `{ "PONG", _handle_pong_command }` in the
connected command table; `irc.cpp:1299-1334` implements latency tracking from the
client's PONG (it validates the token against `conn_get_ircping`).

v3 IRC: `fsm.cpp:84-118` (`handle`) dispatch table has PING, QUIT, MOTD, PASS,
NICK, USER, JOIN, PART, PRIVMSG, NOTICE, AWAY, WHOIS, WHO, MODE, TOPIC, NAMES,
KICK, LIST — **no PONG**. A client PONG therefore falls through to
`421 ERR_UNKNOWNCOMMAND` (`fsm.cpp:117`).

Divergence: server sends PING (the original `irc_send_ping`), but v3 IRC has no
handler for the client's PONG reply → it answers every keepalive PONG with a 421
error. Many IRC clients log/announce unexpected 421s.

(v3 WOL *does* handle PONG: `wol_fsm.cpp:182` ignores it — closer to correct.)

Proposed fix: add `if (c == "PONG") return on_pong(msg);` (even a no-op that
returns `core::ok()`), matching the original's tolerant handling.

---

## FINDING 7 — Server PING line format / keepalive not emitted by FSM

Severity: LOW
Classification: UNSURE (likely INTENTIONAL — out of FSM scope)

Original: `irc.cpp:133-167` (`irc_send_ping`) emits `PING :<servername>` (or
`PING :<token>` pre-login) with `\r\n`. Driven by a latency timer.

v3: no equivalent server-initiated PING found in the IRC/WOL FSM TUs. The FSM only
*responds* to client PING (`on_ping`). Keepalive may be handled by a transport
layer outside the protocol module.

Proposed fix: confirm keepalive PING is emitted elsewhere; if not, parity gap for
clients that rely on server PING to keep NAT alive.

---

## FINDING 8 — WOL welcome sequence: missing RPL_CREATED/RPL_MYINFO/RPL_ISUPPORT/MOTD body

Severity: MEDIUM
Classification: BUG (parity) / INTENTIONAL (skeleton)

Original IRC welcome `handle_irc.cpp:140-200` sends the full ladder:
001 RPL_WELCOME, 002 RPL_YOURHOST, 003 RPL_CREATED, 004 RPL_MYINFO,
005 RPL_ISUPPORT (with `NICKLEN/TOPICLEN/CHANNELLEN/PREFIX/CHANTYPES/NETWORK/IRCD`),
then `irc_send_motd` (375/372.../376), then notices. WOL welcome
(`handle_wol.cpp:268-281`) routes through APGAR auth then the same `irc_send_*`
machinery.

v3 WOL auth success (`wol_auth.cpp:127-139` and skeleton `:143-155`) sends only:
001, 002, 375, 376. v3 IRC `try_complete_registration` (`irc_registration.cpp:71-75`)
sends only 001 RPL_WELCOME.

Divergence: v3 omits 003/004/005 and the ISUPPORT token line. Real clients
(mIRC, Westwood) parse 005 ISUPPORT for `PREFIX`, `CHANTYPES`, `NICKLEN`; absence
changes channel/prefix handling. The RPL_ISUPPORT string and `CHANNEL_PREFIX
"(ohv)@%+"` / `CHANNEL_TYPE "#"` constants are defined in original
`irc_protocol.h` but have no v3 equivalent emitted.

Proposed fix: emit 003/004/005 in the registration completion path with the same
ISUPPORT tokens; restore MOTD body lines (372) from motd file.

---

## FINDING 9 — WOL PART echo prefix lacks user!ident@host form

Severity: LOW
Classification: BUG

Original PART echo is produced via `irc_message_format` case `message_type_part`
(`irc.cpp:687-693`) using `irc_message_preformat(&from, "PART", "\r", channel)`
where `from` = `nick!ctag@ip` → wire `:nick!TAG@1.2.3.4 PART #chan`.

v3 WOL `on_part` (`wol_chat.cpp:201-205`):
```
:<nick> PART <chan>      (no !user@host)
```

Divergence: prefix is bare `:nick` instead of `:nick!user@host`. v3 WOL JOIN
(`wol_chat.cpp:127-131`) *does* use `nick!nick@Battle.net`, so PART is
inconsistent even within v3. Clients that track membership by full prefix may not
match the PART to the JOIN.

Proposed fix: use `:<nick>!<nick>@<host> PART <chan>` for the PART echo (mirroring
the JOIN echo), and match the original's host (client IP), not `Battle.net`.

---

## FINDING 10 — JOIN/PART host token differs (`@pvpgn` / `@Battle.net` vs client IP)

Severity: LOW
Classification: BUG (parity) / UNSURE

Original JOIN echo host = `addr_num_to_ip_str(conn_get_addr(me))`
(`irc.cpp:658`) → the client's real IP; user field = client tag string
(`irc.cpp:657`, `from.user = ctag`). For WOLv2 the JOIN also carries
`:clanID,longIP #channel` (`irc.cpp:678-679`).

v3 IRC JOIN echo (`irc_commands.cpp:127`): `nick + "!" + nick + "@pvpgn"`.
v3 WOL JOIN echo (`wol_chat.cpp:130-131`): `nick + "!" + nick + "@Battle.net"`.

Divergence: user field is the nick (should be client tag), host is a literal
(`pvpgn`/`Battle.net`) instead of the IP, and the WOLv2 `clanID,longIP` JOIN
extension is entirely absent. The original deliberately formats WOL JOIN as
`user!WWOL@hostname JOIN :clanID,longIP channelName` (`irc.cpp:668-679`); v3 emits
`JOIN :#channel` with none of that — Westwood v2 clients will fail to read clan/IP.

Proposed fix: for WOL, populate user=client-tag (`WWOL`), host=server hostname,
and append `:clanID,longIP` trailer for WOLv2; for IRC use client IP as host.

---

## FINDING 11 — WOL PRIVMSG channel `matchbot` / anongame path absent

Severity: LOW
Classification: INTENTIONAL (feature not ported) — noted for completeness

Original `handle_wol.cpp:392-395` routes `PRIVMSG matchbot ...` to
`anongame_wol_privmsg`. v3 WOL `on_privmsg` has no matchbot branch; matchbot is
also injected into NAMES in the original (`irc.cpp:1008-1016`). Anongame/matchbot
is a large unported subsystem — flagged, not a parity bug to fix now.

---

## Things that MATCH (verified correct)

- Line framing: original truncates/parses on `\n`, tolerates `\r`
  (`handle_irc_common.cpp:328-348`). v3 IRC codec `try_parse_line`
  (`codec.cpp:27-38`) finds `\n`, strips optional preceding `\r` — MATCHES. v3 WOL
  `process_lines` (`wol_fsm.cpp:136-167`) handles `\r\n` and bare `\n` — MATCHES.
- CRLF on output: original appends `"\r\n"` (`irc.cpp:109` etc.); v3 IRC
  `encode` appends `"\r\n"` (`codec.cpp:111`); v3 WOL `send_raw` appends `"\r\n"`
  (`wol_fsm.cpp:240`) — MATCHES.
- Max line length: original `MAX_IRC_MESSAGE_LEN` (512) truncation
  (`handle_irc_common.cpp:182`); v3 WOL `kMaxLineLen = 512` (`wol_fsm.cpp:39`).
  Buffer guard differs (v3 hard-closes at 4×512) but the 512 limit MATCHES.
- Trailing `:` param parsing: original splits first `<command>`, then treats a
  param beginning with `:` (or ` :`) as the trailer (`handle_irc_common.cpp:206-222`).
  v3 codec `decode` (`codec.cpp:69-83`) and WOL `extract_trailing`
  (`wol_fsm.cpp:77-81`) implement the same `:`-trailer rule — MATCHES (structurally).
- Command keyword case-insensitivity: original `strcasecmp`
  (`handle_irc.cpp:119`); v3 uppercases the command (`codec.cpp:66`,
  `wol_fsm.cpp:73`) then compares against upper literals — MATCHES.
- Command verbs present: NICK/USER/PASS/PING/PRIVMSG/QUIT/JOIN/PART/NOTICE/WHO/
  WHOIS/MODE/TOPIC/NAMES/KICK/LIST all dispatched in v3 IRC (`fsm.cpp:93-114`).
- Numeric values themselves are correct where emitted: 001/002/375/376 welcome,
  321/322/323 LIST, 331/332 TOPIC, 353/366 NAMES, 315/352 WHO, 311/318 WHOIS,
  401/403/404/421/431/432/451/461/464/474/482 errors — all match the
  `irc_protocol.h` defines. The bug is in the *argument layout* (Finding 1), not
  the code numbers.
- WOL command-set recognition: v3 `wol_known[]` (`wol_fsm.cpp:192-201`) lists the
  same WOL verbs as the original `wol_*_command_table` (CVERS, VERCHK, APGAR,
  SETOPT, SERIAL, GAMEOPT, STARTG, JOINGAME, FINDUSER, PAGE, ADVERTR, CHANCHK,
  GETBUDDY/ADDBUDDY/DELBUDDY, HOST, INVMSG, USERIP, etc.). They are accepted (no
  421) but not implemented — INTENTIONAL skeleton.

---

## Priority summary

| # | Severity | Title | Class |
|---|----------|-------|-------|
| 1 | HIGH | make_numeric drops implicit nick slot | BUG |
| 2 | MED | 353 membership content (flags/names) | BUG/incomplete |
| 3 | MED | WOL QUIT missing RPL_QUIT 607 (sends ERROR) | BUG |
| 4 | LOW | 401 trailer text "No such nick" vs "No such user" | BUG (text) |
| 5 | MED | PRIVMSG underflow → 411/412 instead of 461 | BUG |
| 6 | MED | IRC PONG keyword missing → 421 on every PONG | BUG |
| 7 | LOW | server-initiated PING not in FSM | UNSURE |
| 8 | MED | WOL/IRC welcome omits 003/004/005 ISUPPORT + MOTD | BUG/skeleton |
| 9 | LOW | WOL PART echo lacks user!ident@host prefix | BUG |
| 10| LOW | JOIN/PART host = literal not IP; no WOLv2 clan/IP | BUG/UNSURE |
| 11| LOW | WOL matchbot/anongame path absent | INTENTIONAL |

Highest-impact, most clearly-a-bug: **Finding 1** (numeric argument layout) and
**Finding 6** (PONG → 421), both of which affect real IRC clients on every session.

## Wave 53: WOL kick-old-login
WOL auth must enforce the original's single-session policy by KICKING the old
session, not silently ignoring the attach failure. Verified differentially
(tests/diff/diff_wol_concurrent_login.py): the oracle closes the first session
when the same account logs in again; v3 left both alive. Fixed in wol_auth.cpp by
mirroring the BNCS W3 kick path (session_for -> detach old + message_router_->
disconnect old -> attach new). The kicked WOL session's channel/game cleanup then
runs through the existing on_close path (w51).
