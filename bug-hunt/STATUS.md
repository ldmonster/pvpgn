# Bug Hunt — Status (final summary)

## Wave 133 (LANDED) — BNFTP CLIENT_FILE_REQ filename must be NUL-terminated
DIVERGENCE (bncs-opcode). A CLIENT_FILE_REQ whose filename is NOT NUL-terminated
within the declared packet size diverged. Oracle: packet_get_str_const
(src/common/packet.cpp) scans for a NUL within the packet size and returns NULL
when none is found; handle_file_packet (src/bnetd/handle_file.cpp) then logs
"missing or too long filename" and returns -1 WITHOUT calling file_send, so the
server sends NOTHING. v3: BnftpFsm::try_dispatch used strnlen(fname_start,
fname_max) and, with no NUL present, took ALL remaining packet bytes as the
filename and served the file. Decisive observable (real 38-byte f38.bin, name
'f38.bin' filling the packet to its end with NO trailing NUL): oracle returns 0
bytes + close; v3 returned the full 70-byte reply. Control (same name, properly
NUL-terminated) -> identical 70-byte reply on both. FIX
(src/protocol/file/src/bnftp_fsm.cpp): after fname_len = strnlen(...), if
fname_len == fname_max (no NUL found within the packet) emit no reply, set
state_=Done, ctx_->close(), return core::ok() — matching the oracle's NULL path;
only construct the filename and serve when a NUL was actually found. New guard
tests/diff/diff_bnftp_nonul.py asserts both servers serve the NUL-terminated
control (70 bytes) and both send zero bytes for the unterminated request.
3203/3203 unit tests green (one parallel-run TcpAcceptor adopt flake passes
isolated); diff_bnftp / diff_bnftp_missing still match the oracle.

## Wave 121 (LANDED) — SID_STARTGAME1/STARTGAME3 status=DONE must not ACK
DIVERGENCE (bncs-opcode). BnetFsm::on(StartGame1Request)/on(StartGame3Request)
ignored the inbound `status` field and UNCONDITIONALLY sent a SID_STARTGAME1
(0x08)/SID_STARTGAME3 (0x1A) ACK. The oracle (_client_startgame1/3,
handle_bnet.cpp:4055-4180) only ACKs when the conn has NO current game AND
(status & 0x0f) != CLIENT_STARTGAME{1,3}_STATUS_DONE (0x0c); a DONE status with
no game is logged ("client tried to set game status DONE to destroyed game") and
gets NO reply, and an already-hosted game just updates its status silently.
Decisive observable post full-login: status=DONE(0x0c) -> oracle 0 packets, v3
sent a spurious 0x08/0x1A ACK (body 01000000). FIX (fsm_game.cpp): before the
ACK path in both handlers, return core::ok() with no reply when
current_game_id_ != 0 or (status & 0x0f) == 0x0c. status=OPEN(0x04) still ACKs
(unchanged). New guard tests/diff/diff_startgame_done.py asserts DONE silent +
OPEN acked on both servers for STARTGAME1 and STARTGAME3. 3199/3199 unit tests
green; diff_gamelist / diff_game_disconnect still match the oracle.

## Wave 115 (LANDED) — WOL ladder verbs (HIGHSCORE/LISTSEARCH/RUNGSEARCH) must destroy the connection
DIVERGENCE (connection lifecycle). The oracle's WOL ladder-server handlers
(handle_wol.cpp) close (destroy) the client connection in backend-independent
cases where v3 silently accepted and stayed connected (these verbs lived in the
wol_known[] no-op list): HIGHSCORE (handle_wol.cpp:1854) has its whole body
commented out and UNCONDITIONALLY conn_set_state(conn_state_destroy);
LISTSEARCH (1669) destroys when numparams<1 || !params[0] || !text; RUNGSEARCH
(1730) destroys when numparams<4. Decisive observable: on a logged-in WOL conn,
after a bare ladder verb the oracle drops the socket (no PONG to a following
PING), while v3 returned core::ok() and answered the PING. FIX (wol_fsm.cpp +
wol_fsm/wol_chat.cpp): removed the three verbs from wol_known[], added explicit
handlers on_highscore (always Disconnecting + ctx_->close(), mirroring the
unconditional destroy), on_listsearch (close when no first param or no trailing
text; success path needs an unimplemented ladder backend -> no-op), on_rungsearch
(close when fewer than 4 middle params -> no-op otherwise). New helper
split_irc_params mirrors handle_irc_common_line's middle-param/trailing split
(trailing begins at leading ':' or first " :", not counted in numparams).
SERIAL/ADVERTC/INVDEL stay no-ops (they genuinely no-op in the oracle). New guard
tests/diff/diff_wol_ladder_destroy.py asserts BOTH servers drop the connection
after each bare ladder verb while a control PING/TIME keeps it alive (PASS).
3199/3199 unit tests green; diff_wol_advertr/chat/disconnect still match.


Reference: `/home/cnupt/work/pvpgn-server` (upstream PvPGN-PRO) vs this repo's v3
rewrite. 25 subsystems analyzed by a discovery fleet (one findings file each under
`findings/`), triaged by the orchestrator, with confirmed *implemented-but-wrong*
bugs fixed + regression-tested. Full unit suite green after every fix.

## Wave 110 (LANDED) — WOL PING -> PONG 512-byte length guard (oversized PONG suppressed)
DIVERGENCE (wire over-length line). The original irc_send_pong (src/bnetd/irc.cpp:169)
computes the full constructed reply length and, when it would exceed
MAX_IRC_MESSAGE_LEN (512), suppresses the reply entirely (logs "max message length
exceeded", returns -1, sends nothing). v3's WolFsm::on_ping (wol_fsm/wol_chat.cpp)
always built and sent the PONG with no length check, so a long PING token produced an
over-length PONG line. FIX: mirror the oracle's exact arithmetic before send_raw —
suppress (return core::ok()) when
(1 + hostlen + 1 + 4 + 1 + hostlen + (token ? 2+tokenlen : 0) + 2 + 1) > 512.
Decisive hostname-independent case: a 600-char PING token -> both servers now send 0
bytes (v3's own constructed PONG ~627B clearly exceeds 512). Pinned by
tests/diff/diff_wol_ping_overlong.py (short token = both PONG control; 600-char token =
both suppress). Distinct from wave 93 (PONG wire FORMAT) and the line-truncation cap.

## Wave 104 (LANDED) — WOL TOPIC uses the IRC trailing param (extra middle params discarded)
DIVERGENCE (wrong topic text), another WOL fuzz item flagged in wave 93. A real
IRC line is "<cmd> <middle params...> :<trailing>". The original's line tokeniser
(handle_irc_common.cpp) splits a TOPIC line into middle params plus a single
trailing ":" parameter, and _handle_topic_command sets the channel topic from that
TRAILING text alone — any extra middle params between the channel name and the ":"
are discarded. v3's WolFsm::on_topic (wol_fsm/wol_chat.cpp) instead took
everything after the channel TOKEN as the topic, so `TOPIC #c extra :the topic`
stored the topic as "extra :the topic" instead of "the topic". Probe:
`TOPIC #Lob extra :new topic here` -> oracle "332 ... #Lob :new topic here",
v3 "332 ... #Lob :extra :new topic here".
Fix: on_topic now extracts the topic as the IRC trailing parameter (the text after
the first " :" — verbatim, no trim, mirroring the original's raw post-colon
pointer). When there is no trailing " :" there is no topic to set, so we fall
through to the safe QUERY path; the original instead CRASHES on a colon-less
"TOPIC #c" (std::string(NULL) deref), which v3 deliberately does not replicate.
The normal single-param form (`TOPIC #c :text`) is unaffected.
New guard tests/diff/diff_wol_topic_extraparam.py: `TOPIC #TX extra :TheTopic`
must store/echo only "TheTopic" (332 set echo + persisted 332 seen by a later
joiner) on both servers — match (before: v3 stored "extra :TheTopic"). 3199/3199
units green; WOL diff regression (topic, topic_notonchan, chat, part, kick, mode)
all still match the oracle.

## Wave 103 (LANDED) — WOL TOPIC for a channel you're not on -> 442 (was a 332 echo)
DIVERGENCE (wrong reply numeric), one of the WOL fuzz items flagged in wave 93.
The original (_handle_topic_command, irc.cpp, WOL branch) persists the topic on
the client's CURRENT channel using the trailing text REGARDLESS of which channel
name was typed, then derives its reply from the query path: it answers 332
RPL_TOPIC only when the typed channel matches the channel the client is on,
otherwise 442 ERR_NOTONCHANNEL "<name> :You're not on that channel" (the same 442
it gives off-channel). v3's WolFsm::on_topic (wol_fsm/wol_chat.cpp) instead always
echoed 332 with the typed name, so `TOPIC #other :x` while sitting in #lobby got a
bogus success echo. Probe: in #LobbyA, `TOPIC #OtherChan :Hello` -> oracle "442
... :You're not on that channel", v3 "332 ... :Hello".
Fix: on_topic SET path now compares the typed channel (case-insensitive, iequals)
to the current channel_; on a mismatch (or off-channel) it answers 442. The
quirky shared side effect is preserved — the foreign-name SET still updates the
current channel's topic (both servers do this; verified visible to a later
joiner's 332), so only the reply numeric changed. The same-channel happy path
(332 + persisted topic on join, diff_wol_topic) is untouched. (The iequals helper
was relocated above on_topic so it is in scope; its existing callers are
unaffected.) The bare-query path is left as v3's safe 332 — the oracle CRASHES on
"TOPIC #chan" (std::string(NULL)) so there is no behavior to match there.
New guard tests/diff/diff_wol_topic_notonchan.py: foreign-channel TOPIC -> 442
(not 332), current-channel TOPIC -> 332, on both servers — match (before: v3 gave
332 for the foreign channel). 3199/3199 units green (incl. all 63 wol units); WOL
diff regression (topic, mode, kick, part, chat, login, names) all still match.

## Wave 102 (LANDED) — BNFTP missing/unsafe file sends NO reply (was a phantom header)
DIVERGENCE (spurious reply). A BNFTP CLIENT_FILE_REQ for a file that does not
exist — or a rawname containing a path separator — makes the original's
file_send (src/bnetd/file.cpp) throw inside file_get_info (stat() fails, or the
'/'/'\\' guard fires) and return -1 BEFORE pushing any packet, so the oracle
sends NOTHING and just lingers/closes. v3's BnftpFsm::handle_file_request
(protocol/file/src/bnftp_fsm.cpp) instead emitted a phantom zero-length
SERVER_FILE_REPLY header (filelen=0, echoed filename) for BOTH the not-found and
unsafe-filename cases — a 47-byte reply a real client never sees from the oracle.
Probe: request "does_not_exist.bin" -> oracle 0 bytes, v3 47-byte header.
Fix: both error branches now `return core::ok()` with no send; the FSM still
sets Done + closes (the same minor v3-closes-vs-oracle-lingers difference already
accepted in diff_bnftp.py). Happy path (real file serve, resume, long name)
untouched. Updated 4 unit tests that asserted the old zero-length reply to assert
NO reply (file-not-found, two path-traversal, split-reassembly) plus the
app-level not-found session test.
New guard tests/diff/diff_bnftp_missing.py: missing file + '/'+'\\' traversal +
"subdir/..." all return ZERO reply bytes on both servers (before: v3 sent a
header). 3199/3199 units green; BNFTP diff regression (bnftp, resume, longname,
fileinfo) + diff_chat all still match the oracle.

## Wave 101 (LANDED) — WOL PRIVMSG-to-ghost 401 wire form (":No such user")
DIVERGENCE (wrong reply), from the wave-100 WOL fuzz-leftover list. A post-login
WOL whisper to a non-online nick ("PRIVMSG ghostuser :hi") takes the original's
whisper branch (_handle_privmsg_command, handle_wol.cpp): target has no '#',
connlist_find_connection_by_accountname misses, and it replies
irc_send(conn, ERR_NOSUCHNICK, ":No such user") -> the wire line
":<server> 401 <nick> :No such user" (NO target middle param; text "No such
user"). v3's WolFsm::on_privmsg (wol_fsm/wol_chat.cpp) instead did
send_numeric(401, nick_, tgt + " :No such nick"), producing
":... 401 <nick> :<target> :No such nick" — a stray colon, the target echoed,
and the wrong text. A stale code comment even claimed it already matched the
original. Probe: oracle "401 pmprober :No such user"; v3
"401 pmprober :ghostuser :No such nick".
Fix: on_privmsg's user-target branch now does send_numeric(401, nick_,
"No such user"), matching the original byte-for-byte; comment corrected. (v3 has
no nick->session whisper routing yet, so an online recipient also falls to this
miss path; the miss-form bytes are the cleanly-diffable part and are now right.)
Updated the unit test "PRIVMSG to nick contains target nick in 401 reply" (which
pinned the WRONG form) to assert the oracle form (':No such user' present, target
absent). New guard tests/diff/diff_wol_privmsg_nouser.py: server-name-stripped
401 line matches the oracle (before: v3 stray-colon + wrong text). 3199/3199
units green; WOL diff regression (chat, nosuchnick, login, part, robustness)
all still match the oracle.

## Wave 100 (LANDED) — WOL JOIN of a non-'#'-prefixed name -> 403 "JOIN failed"
DIVERGENCE (silently auto-creates where oracle rejects). The original
(irc.cpp _handle_join_command) runs the JOIN target through irc_convert_ircname(),
which returns NULL for any name not prefixed with '#' (or '!<id>'); a NULL ircname
makes the server answer ERR_NOSUCHCHANNEL ":<server> 403 <nick> <name> :JOIN
failed". v3's WolFsm::on_join (wol_fsm/wol_chat.cpp) instead stripped a leading
'#' (a no-op for a bare token) and called the JoinChannel use-case, which
auto-created a brand-new channel from the token and echoed a full JOIN/353/332/366
welcome — letting a malformed JOIN spawn junk channels the original would reject.
Probe: "JOIN plainname" -> oracle "403 uj plainname :JOIN failed"; v3 auto-created
"plainname" and joined it.
Fix: on_join now, right after the empty-name 461 guard, rejects any target whose
first char is not '#' (excluding the RFC2812 "JOIN 0" part-all sentinel) with
send_numeric(403, "<nick> <name>", "JOIN failed") — the name carried as a middle
param exactly like the original. The normal '#'-prefixed happy path is untouched.
New guard tests/diff/diff_wol_join_unprefixed.py: "JOIN plainname" returns
403 ... plainname :JOIN failed (server-name-stripped, byte-for-byte) on both
servers, and a following "JOIN #realchan" still echoes JOIN + 366 — match (before:
v3 auto-created). 3199/3199 units green; WOL diff regression (login, chat, part,
names, list, topic, mode, kick, needmoreparams, nosuchnick, joingame, lobby) +
diff_robustness all still match the oracle.
Other still-open WOL fuzz divergences (each a separate behavior, not a crash):
PRIVMSG to a logged-in user/self (oracle delivers a whisper; v3 always 401s, and
its 401 miss form ":nobody :No such nick" diverges from the original's bare
":No such user"); TOPIC with an extra middle param (v3 folds the extra token into
the stored topic). Left for follow-up — user-whisper needs nick->session routing.

## Wave 99 (LANDED) — removed dead ServerMetrics aggregator (infra/metrics)
Dead-code removal. `infra/metrics/src/server_metrics.cpp` +
`include/infra/metrics/server_metrics.hpp` defined `ServerMetrics` (a struct of
shared_ptr metric handles + a single static `ServerMetrics::create(registry)`
factory that registers ~15 named server metrics). Proven dead: grep over src+tests
showed `ServerMetrics`/`server_metrics.hpp` referenced NOWHERE except its own two
files — no test, no consumer. The metrics subsystem itself is LIVE: bnetd main.cpp
wires `InMemoryMetricsRegistry` + `HttpMetricsServer` directly, but never calls
`ServerMetrics::create()`, so the aggregator was orphaned scaffolding within an
otherwise-used lib. Removed the .cpp/.hpp + the `infra/metrics/src/server_metrics.cpp`
SOURCES line in src/CMakeLists.txt (infra_metrics keeps in_memory_metrics_registry
+ http_metrics_server). Reconfigure + full relink + 3199/3199; the metrics
registry test (test_infra_metrics_registry) still passes; diff_chat still matches
the oracle (bnetd behaviorally unchanged — it never referenced ServerMetrics).

## Wave 98 (LANDED) — WOL MODE channel-query faithfulness (403/472 vs 324 echo)
DIVERGENCE (wrong reply), from the wave-93 fuzz-leftover list. v3's WolFsm::on_mode
(wol_fsm/wol_chat.cpp) channel branch echoed 324 RPL_CHANNELMODEIS "+tns" for ALL
channel MODE queries regardless of membership, and treated "+b" as a ban-list query.
The original _handle_mode_command (irc.cpp) channel branch instead:
  - 403 ERR_NOSUCHCHANNEL "<#c> :No such channel" when the client is not on a channel
    (it dereferences conn_get_channel(conn) first);
  - 324 "+tns" only for numparams==1 ("MODE #c");
  - 368 RPL_ENDOFBANLIST only for the literal single token "b" ("MODE #c b");
  - 472 ERR_UNKNOWNMODE ":<mode> is unknown mode char to me for <#c>" for any other
    single mode token (incl. "+b", "+z", ...).
Probe confirmed: oracle gives 403 for MODE #X before JOIN, 472 for MODE #M +z and
MODE #M +b after JOIN; v3 gave 324 (or 368 for +b). Fix tokenizes the post-channel
params to count them (params[1]/params[2]), gates on channel_id_ for membership, and
emits 403/472/368/324 exactly like the original. numparams>=3 mode *changes* (op/ban
/limit) are still not wired through here (benign 324 echo as before).
New guard tests/diff/diff_wol_mode.py: off-channel 403 (query + char), on-channel
324/368/472(+z)/472(+b) all match the oracle. 3199/3199 units green; WOL diff
regression (timemode, login, part, names, kick, chat) all still match the oracle.

## Wave 97 (LANDED) — SID_MOTD_W3 (0x46) welcome reply (was a silent no-op)
DIVERGENCE (silent where oracle replies), found auditing an untested BNCS opcode
the v3 FSM "handles". A logged-in WarCraft III client sends CLIENT_MOTD_W3 (0x46,
body = u32 last_news_time) and BLOCKS for the response. The original
(_client_motdw3, handle_bnet.cpp, logged-in handler table) answers with zero or
more news entries (one SERVER_MOTD_W3 each) followed by a single "welcome" packet
whose timestamp2 == SERVER_MOTD_W3_WELCOME (0). v3's BnetFsm::on(MotdRequest)
(fsm_chat.cpp) was a bare `require_clan_state(...)` no-op and sent NOTHING — a W3
client stalled after login. The MotdReply message struct + encoder + variant
registration already existed (codec_ladder.cpp); only the FSM wiring was missing.
Probe: post-login 0x46 -> oracle sends 2 packets (a "No news today" news entry +
an empty-text welcome with timestamp2=0), v3 sent zero.
Fix: on(MotdRequest) now (after the login gate) builds and sends one MotdReply
welcome packet: msg_type=1, curr_time=std::time(now), first_news_time=0,
timestamp=1 (first_news_time+1, matching the original's welcome math),
timestamp2=0 (WELCOME marker), empty text (v3 has no news subsystem / bnmotd_w3
file — equivalent to a no-news oracle config). Also corrected the stale header
doc-comment opcode (SID_MOTDREQ 0x1A -> SID_MOTD_W3 0x46).
New guard tests/diff/diff_motd_w3.py: compares config-independent observables
(the harness oracle carries a stock news entry, so news-packet count/timestamps
are intentionally not compared) — both servers send >=1 0x46, both send a welcome
packet (timestamp2==0) with msg_type==1 and a plausible (>0) curr_time. Match
(before: v3 sent nothing). 3199/3199 units green; diff regression
(w3_login, realmlist, chat, whoami, channelcmds) all still match the oracle.

## Wave 96 (LANDED) — WOL QUIT reply wire format (607 RPL_QUIT :goodbye)
DIVERGENCE (wrong reply), connection-lifecycle. The original _handle_quit_command
(handle_wol.cpp) answers a client QUIT with the WOL-specific numeric RPL_QUIT
(607) — irc_send formats it ":<server> 607 <nick> :goodbye" — then destroys the
connection (the channel PART to the other members already flows from
conn_quit_channel, which v3 mirrors in WolFsm::on_close). v3's WolFsm::on_quit
(wol_fsm/wol_chat.cpp) instead sent a bare "ERROR :Closing Link" that no real WOL
client expects. Probe: WOL login then QUIT -> oracle ":<host> 607 quitter
:goodbye", v3 "ERROR :Closing Link".
Fix: on_quit now sends send_numeric(607, nick_ (or "*"), "goodbye") before
ctx_->close(), matching the original byte-for-byte. Updated the one unit test that
asserted the old ERROR line (wol_fsm_test.cpp "QUIT sends ERROR line" -> asserts
" 607 " + ":goodbye") and the wol_chat.cpp header doc-comment.
New guard tests/diff/diff_wol_quit.py: server-name-normalized QUIT reply matches
the oracle (before: v3 sent ERROR). 3199/3199 units green; WOL diff regression
(login, chat, part, disconnect, list_params, ping, topic, kick) all still match.

## Wave 95 (LANDED) — WOL LIST param-count gating (channels vs. empty envelope)
DIVERGENCE (over-listing), follow-up to the wave-93 WOL fuzz sweep ("LIST with
extra params -> oracle returns empty list"). The original _handle_list_command
(handle_wol.cpp) decides what to list from the COUNT and EQUALITY of the middle
params: numparams==0 lists chat channels (and games); numparams==2 with unequal
params lists channels only; numparams==2 with EQUAL params lists games only (no
channels); any other count (1, 3, ...) lists NEITHER — just the 321/323 envelope.
v3's on_list (wol_fsm/wol_chat.cpp) ignored params entirely and always emitted the
channel set, so "LIST 0", "LIST 0 0" and "LIST 0 0 0" wrongly returned channels
where the oracle returns just 321+323. (Common real clients like Dune2000 send
"LIST 0 0", which the oracle treats as games-only.)
Fix: on_list now counts the middle params the way irc_get_paramelems does (tokens
before any " :" trailing marker) and gates the channel-emitting block on
list_channels = toks.empty() || (toks.size()==2 && toks[0]!=toks[1]). v3 has no
WOL game listing wired, so the games-only case collapses to the empty envelope —
which matches the oracle whenever no games exist (the differential case here).
Probe across LIST / LIST 0 / LIST 0 0 / LIST 0 1 / LIST 0 0 0: before v3 listed
the user channel in all five, after it matches the oracle (present only for the
bare and two-unequal forms).
New guard tests/diff/diff_wol_list_params.py: joins #PListCh and asserts the
channel's 327 entry appears for bare + two-unequal and is absent for one/two-equal/
three — match. 3199/3199 units green; WOL diff regression (list, chat, login,
names, part, topic, kick) all still match the oracle.

## Wave 94 (LANDED) — WOL 401 ERR_NOSUCHNICK wire format (target is a middle param)
DIVERGENCE (wrong wire format), follow-up to the wave-93 WOL fuzz sweep. Four WOL
command handlers (USERIP/HOST/GAMEOPT/ADDBUDDY) reply with ERR_NOSUCHNICK when
their target nick is not an online user. The original (handle_wol.cpp) builds each
as irc_send(ERR_NOSUCHNICK, "<target> :No such nick"), so irc_send_cmd emits
":<server> 401 <nick> <target> :No such nick" — the target nick is a MIDDLE
parameter with NO leading ':'. v3's on_userip/on_host/on_gameopt/on_addbuddy
(wol_fsm/wol_chat.cpp) instead passed the target into send_numeric's trailing-text
field, which always injects " :", producing ":... 401 <nick> :<target> :No such
nick" — a stray ':' before the target nick on every miss. A WOL client parsing
the 401 saw a malformed reply (the target appeared as the trailing text, prefixed
by ':'). Probe (USERIP/HOST/GAMEOPT/ADDBUDDY ghost): oracle "401 <nick> ghostx
:No such nick", v3 "401 <nick> :ghostx :No such nick" on all four.
Fix: the four call sites now pass the target as a middle param
(send_numeric(401, nick_ + " " + target, "No such nick")), exactly like the
existing send_needmoreparams helper. on_privmsg's user-target 401 is INTENTIONALLY
left unchanged — the original PRIVMSG whisper-miss path uses a different text
(":No such user", no target) and v3's PRIVMSG user path has a separate, larger
gap (it 401s where the original whispers to an online user) deferred to its own
wave; a code comment records this.
New guard tests/diff/diff_wol_nosuchnick.py: USERIP/HOST/GAMEOPT/ADDBUDDY to a
ghost nick, server-name-stripped 401 lines match the oracle byte-for-byte (before:
v3 diverged on all four). 3199/3199 units green; WOL diff regression (userip,
needmoreparams, needmoreparams2, buddy, page, chat, login, part, list, names,
topic, kick, advertr, gameopt, finduser, invmsg, startg) all still match the
oracle.
Other wave-93 fuzz-surfaced WOL divergences still NOT fixed (each a separate
behavior, none crash): PRIVMSG to an online user (oracle whispers vs v3 401),
PRIVMSG to self (oracle echoes vs v3 401), JOIN of an unprefixed name (oracle 403
vs v3 auto-creates), MODE +unknownchar (oracle 472 vs v3 324 echo), TOPIC with
extra middle param (oracle 442 vs v3 sets), LIST with extra params (oracle empty
list).

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

## Wave 27: /who, /whois, /users channel info commands
/who hit a "no registry" stub; /whois and /users did nothing. Implemented as FSM
interceptors (like /whisper, /me, /friends) via a new channel_reader
(IChannelReader, wired in make_use_case_context from the shared channel repo):
/who <channel> lists members; /whois <user> reports the user's current channel (or
offline); /users gives population stats. diff_channelcmds.py: /who member set
matches the oracle (alice, bob). NOTE: the original localize()s + charset-converts
its INFO replies; in the harness (mock, no codepage) i18n_convert garbles the
LOCALIZED text — the /who prefix and the whole /whois come back mangled. The /who
member names are sprintf'd outside localize so they survive and are the comparable
observable; v3's clean /whois is verified v3-side. Fixed the R306 /who test
contract (no-arg → EID_INFO usage, matching the oracle's describe_command).

## Wave 28: /squelch /unsquelch + broadcast-side ignore filtering
/squelch was unimplemented and squelched users' messages were never filtered.
New IIgnoreStore (application/chat) + InMemoryIgnoreStore (infra/inmemory),
run-loop wired (ignore_store ctx field). handle_squelch intercepts /squelch
/ignore (+ /unsquelch /unignore) like /whisper. filter_squelched() drops, from a
TALK/EMOTE broadcast's recipients, any session whose account ignores the sender
(session_registry->account_for + ignore_store) — mirrors the original MF_X
delivery filter; no-op when nothing is ignored. diff_squelch.py: bob's channel
message reaches alice BEFORE /squelch and is suppressed AFTER, matching the
oracle on both (decisive observable is delivery, not the garbled reply text).
New drain_chat() mock helper.

## Wave 29: WOL post-login lobby (LIST + JOIN)
The WOL chat use-cases were never wired into the production WOL session, and
on_list emitted the standard IRC 322 instead of WOL's 327 RPL_CHANNEL. Wired
ListChannels/JoinChannel/PostMessage into make_wol_session; rewrote on_list to
emit 327 "<name> <count> <official> 388" (WOLv2) via send_raw. WOL JOIN now
persists to the shared channel repo; LIST enumerates it. diff_wol_lobby.py: a WOL
client joins #wollobby and LISTs — the channel appears on both servers. Updated
R307 WOL LIST unit tests to the 327 format.

## Wave 30: re-harden the expanded surface (ASan + UBSan fleet)
Per user "iterate until 100% hardened/fixed", ran ASan+libFuzzer and UBSan fleet
agents over the FULL expanded surface (passchange 0x55/0x56, friends 0x65/0x66,
game 0x1C/0x09, all chat commands incl. squelch, WOL pre/post-login LIST/JOIN).
Found+fixed 1 real availability defect: build_config() left bnftp_port at 6112
when -p set a non-6112 BNet port → the dedicated-BNFTP path bound a hardcoded
0.0.0.0:6112 → fatal "Address already in use" (broke non-default ports + 2
instances). Fix: bnftp_port tracks bnet_port (commit 266e836). Post-fix: ~150
malformed-input cases + 25 UBSan batches, 0 sanitizer hits, server alive
throughout; libFuzzer 8.79M execs 0 crashes; full unit suite 3150 green; all 9
oracle differentials pass. Both sanitizers report CLEAN.

## Wave 31: WOL channel chat delivered across sessions (routing foundation)
on_privmsg posted a WOL channel message via PostMessage but DROPPED the returned
recipient SessionId list — WolFsm had no message router and WOL sessions were
never registered with the cross-session MessageRouter, so two WOL clients in one
channel never heard each other (and GAMEOPT/STARTG broadcasts depend on this
path). Fix: make_wol_session assigns each WOL connection a real SessionId +
registers its egress with the shared MessageRouter (unregister on close); new
WolFsm::set_routing; on_privmsg encodes the IRC line once and routes it to the
recipients (no self-echo). Session identity now lines up across the single shared
registry. diff_wol_chat.py matches the oracle. (commit c9c7133)

## Wave 32: fix 2 ASan-found WOL session-lifecycle leaks
ASan over the W31 surface: memory-SAFE (0 UAF/overflow/double-free; the
disconnect-vs-send race is guarded by the router's weak_ptr) but NOT leak-clean.
(a) Pre-existing, protocol-agnostic reference cycle — TcpSession callbacks capture
the FSM, which transitively owns the TcpSession via its egress; close() never
released the callbacks → whole graph leaked on every disconnect (~6KB/conn, 940KB
over 150 conns, all Indirect). Fix: TcpSession::fire_close_and_release() clears
on_close_/on_bytes_ after firing (benefits BNCS/WOL/IRC/BNFTP). (b) WOL attached
account->session on auth but never detached; fix: WolFsm::on_close detaches.
Verified leak-clean via an LSan driver (8 rounds x 2 clients, 0 leaks). (dca75f6)

## Wave 33: WOL GAMEOPT game-option relay
GAMEOPT was a silent no-op. on_gameopt now relays the opaque options: channel mode
(#...) broadcasts ":<nick>!<nick>@Battle.net GAMEOPT <#chan> :<opts>" to current-
channel members (channel_reader threaded into WolFsm + session registry, no
self-echo; mirrors channel_message_send/message_type_gameopt_talk); whisper mode
resolves nick->account->session (401 if offline). Shared route_irc_line() helper.
diff_wol_gameopt.py matches the oracle; re-hardened (extended LSan driver covering
GAMEOPT channel+whisper+malformed + ASan diff: 0 leaks, 0 crashes). (f47c0d2)

## Wave 34: WOL JOINGAME (game-as-channel create/join)
JOINGAME was a silent no-op — WOL clients could never host/join a game lobby.
Added a WOL game-channel registry (application::game::IWolGameStore +
InMemoryWolGameStore, run-loop scoped, shared across WOL sessions) holding the
WOL-specific metadata (min/max players, game type, tournament, gameExtension,
password, host, backing channel id, players) — kept apart from the BNCS
IGameRepository. on_joingame: CREATE (>=7 params) registers the game + creates
the backing channel and acks the host; JOIN (2-3 params) finds the game (478 if
closed), enforces full (471) then password (475) [matches oracle order], joins
the channel, and acks every channel member via the router. diff_wol_joingame.py:
A creates #wolgame, B joins, ack "2 8 1 1 1 0" matches the oracle. (commit 70484ed)

## Hardening (waves 33-34): GAMEOPT + JOINGAME re-hardened to 100%
ASan + UBSan fleet over the new surface. Both report HARDENED 100% / 0 defects:
all 5 WOL differentials (chat/gameopt/joingame/lobby/login) match the oracle
against BOTH sanitizer binaries; WOL unit 63/63 + full suite green (the recurring
anongame/multilocale parallel temp-file race + a pre-existing tomlplusplus UBSan
finding are the only "failures", both non-defects); leak driver (login → JOINGAME
create/join → GAMEOPT → PRIVMSG → disconnect, x8 rounds, SIGTERM) reports 0 leaks;
edge battery (malformed JOINGAME param counts, non-numeric/overflow strtoul
tokens, full/wrong-password joins, invalid-UTF-8, pre-auth) — server stayed up,
0 ASan/UBSan reports.

## Wave 35: WOL FINDUSER / FINDUSEREX presence lookup
Both were silent no-ops. on_finduser resolves nick -> account (account_reader)
and reports presence (WOL findme defaults on, so online == findable): 388/398
"0 :<channel>" found (channel via channel_reader) / "1 :" not. Reply built raw to
match irc_send_cmd framing. diff_wol_finduser.py: online -> 0, unknown -> 1,
FINDUSEREX -> 0, matches oracle. (commit b8f4c39)

## Wave 36: WOL buddy list (GETBUDDY / ADDBUDDY / DELBUDDY)
Silent no-ops. Implemented by reusing v3's existing social use-cases (the SAME
friend-list store as BNCS): ADDBUDDY resolves name -> AddFriend (334, or 401
unknown); DELBUDDY RemoveFriend (335, echoes name regardless like the original);
GETBUDDY 333 backtick-terminated list (ListFriends). Threaded social use-cases
into WolFsm (set_social); protocol_wol now deps application_social.
diff_wol_buddy.py: add shows (334), del removes (335), matches oracle. (f52858c)

## Hardening (waves 35-36): FINDUSER + BUDDY re-hardened to 100%
ASan + UBSan fleet → both "HARDENED 100% / 0 defects": all 7 WOL differentials
(login/lobby/chat/gameopt/joingame/finduser/buddy) match the oracle against BOTH
sanitizer binaries; WOL unit 63/63 + full suite green (only the known parallel
anongame race + pre-existing tomlplusplus finding); leak drivers (buddy/finduser
+ game-lobby rounds) report 0 leaks; hostile edge fuzzing (invalid-UTF-8/long
nicks, duplicate/never-added buddies, self-add, 0-param variants) left the server
alive with 0 ASan/UBSan reports.

## Wave 37: WOL codepage / locale / GETINSIDER
SETCODEPAGE/GETCODEPAGE (329/328), SETLOCALE/GETLOCALE (310/309), GETINSIDER
(399). Values stored per-session and echoed; GET forms emit the backtick
"<nick>`<value>`" payload (own value for this nick, 0 for others — no cross-
session registry, documented). New send_raw_cmd() helper (irc_send_cmd framing);
the finduser/buddy raw replies were refactored onto it. diff_wol_userinfo.py
matches the oracle. (commit 610684a)

## Wave 38: WOL PAGE
PAGE <nick> :<msg> resolves nick -> account -> session, delivers the page via the
router, replies 389 "0 :" (paged) / "1 :" (offline/unknown). pageme defaults on.
diff_wol_page.py matches the oracle. (commit 8b2dcf6)

## Hardening (waves 37-38): re-hardened to 100%
ASan + UBSan fleet over the full WOL command surface (the send_raw_cmd refactor is
shared) → both "HARDENED 100% / 0 defects": all 8 WOL differentials match the
oracle against BOTH sanitizer binaries; leak drivers (incl. codepage/locale/
insider/page rounds) report 0 leaks; hostile edge fuzzing (out-of-range atoi,
invalid-UTF-8/200-char nicks, case-flipped self, malformed PAGE) clean. PAGE
additionally verified leak-clean + oracle-matching against the ASan binary.

## Wave 39: WOL CHANCHK + HOST
CHANCHK <channel> replies ":<server> CHANCHK <channel>" if the channel exists
(channel_reader->find_by_name) else 403. HOST <nick> :<text> relays
":<nick>!<nick>@Battle.net HOST : <text>" to an online target via the router else
401. diff_wol_chanchk_host.py matches the oracle. (commit b9934d4) USERIP left
stubbed (needs peer-IP tracking like STARTG).

## Hardening (waves 38-39): re-hardened the full WOL command surface to 100%
ASan + UBSan fleet over PAGE/CHANCHK/HOST + the whole surface → both "HARDENED
100% / 0 defects": ALL 10 WOL differentials (login/lobby/chat/gameopt/joingame/
finduser/buddy/userinfo/page/chanchk_host) match the oracle against BOTH sanitizer
binaries; leak driver (all commands, graceful SIGTERM) 0 leaks; edge/invalid-UTF-8
fuzzing of the whole surface clean. WOL unit 63/63; full suite green.

## Wave 40: peer-IP registry + WOL USERIP
Added domain::connection::IPeerAddressStore + InMemoryPeerAddressStore (run-loop
scoped, AccountId -> peer IP). make_wol_session captures TcpSession::
remote_endpoint(); WOL login registers it, on_close removes it. USERIP <nick>
resolves target -> account -> IP and replies ":<nick>!<nick>@Battle.net USERIP
<nick> <ip>" (401 if offline/unknown). diff_wol_userip.py: online -> 127.0.0.1,
unknown -> 401, matches the oracle. Hardened: USERIP matches under both sanitizer
binaries; leak driver (incl. the peer-IP set/remove lifecycle) 0 leaks. This
registry also UNBLOCKS STARTG (the per-player IP list). (commit a47e519)

## Wave 41: WOL INVMSG game-invite relay
INVMSG <channel> <flag> <invited,...> relays the invite to each named online user
via the router. The differential caught a real format detail: the original's wire
form carries the invitee's OWN name first — ":<sender>!.. INVMSG <invited>
<channel> <flag>" — so v3 prepends the invitee name per recipient.
diff_wol_invmsg.py matches byte-for-byte ("invb #invroom 1"). Hardened: matches
under ASan; leak driver (incl. INVMSG) 0 leaks. (commit 275215a)

## Wave 42: WOL STARTG — closes the JOINGAME/GAMEOPT/STARTG game lobby
on_startg resolves the sender's game (wol_game_store by channel) and sends each
named player ":<owner>!<owner>@Battle.net STARTG <player> :<owner_ip> <gameid>
<time>" via the router (owner_ip from the peer store, gameid = channel_id, time =
std::time). diff_wol_startg.py is a TOLERANT diff (delivery + owner-IP presence):
the trailing gameNumber/time_t differ per server/run and cannot be byte-compared
— STARTG is the one WOL command whose payload is not byte-diffable. Both servers
deliver STARTG to the named player carrying 127.0.0.1. Hardened: matches under
ASan; leak driver (full create->join->STARTG sequence) 0 leaks. (commit d4c35b9)

## Wave 43: WOL SETOPT (findme/pageme gating)
Added a shared per-account flags store (application::game::IWolUserFlagsStore +
InMemoryWolUserFlagsStore, run-loop scoped, both default ON, removed on close).
SETOPT <find>,<page> (16/17 find off/on, 32/33 page off/on) updates the sender's
flags; FINDUSER now requires the target's findme, PAGE the target's pageme.
diff_wol_setopt.py: across two clients, default 0/0 -> SETOPT 16,32 -> 1/1 ->
SETOPT 17,33 -> 0/0, matching the oracle. Hardened under both sanitizers +
leak-clean. (commit 113c563)

## Wave 44: WOL ADVERTR game-ad refresh ack
ADVERTR <channel> replies ":<server> ADVERTR 5 <channel>" (461 with no param).
diff_wol_advertr.py matches the oracle ("5 #adroom"); hardened under ASan.
ADVERTC remains a no-op (as in the original). (commit 0354fe0)

## Full regression sweep (post-session): 29/29 diff scenarios PASS
Ran the ENTIRE differential suite against the oracle after this session's changes
(every diff_*.py with --v3-bnetd + unique ports): all 13 BNCS scenarios (OLS/W3
login, W3 passchange, chat/talk/whisper/emote/leave/multichannel/channelcmds/
squelch, friends, gamelist), all 15 WOL scenarios, and diff_all_clients (full
supported-client matrix) — 29/29 PASS, 0 regressions. Confirms the wave-32
cross-protocol TcpSession leak fix and all WOL waves left the whole bug-hunt
intact. Runner: scratchpad/run_all_diffs.sh.

## Wave 45: BNCS READUSERDATA/WRITEUSERDATA (account profile)
The 0x26/0x27 handlers were stubs (no reply). Implemented via a new
application::auth::IUserProfileStore (+ InMemoryUserProfileStore, run-loop scoped,
per-account string attributes, in BnetUseCaseContext): WRITEUSERDATA stores
"profile\\" keys on the caller's own account; READUSERDATA replies with
name-major/key-minor values (or "" unset; "BNET\\" hidden cross-account).
diff_userdata.py: write profile\\sex/age/location, read back -> ["m","99","NY",""],
matches the oracle. Hardened: matches under both sanitizer binaries; the codec
already caps cell counts. (commit 7974ade)

## Wave 46: BNCS SID_PROFILE (0x35) profile view ("/finger")
on(ProfileRequest) was a stub. Implemented against _client_profilereq: resolve
the requested account (nonexistent -> no reply, like the original); else reply
ProfileReply{cookie, fail=0, description, location, clan_tag=0} served from the
wave-45 profile store. No timestamps in the reply -> byte-exact diff.
diff_profile.py matches the oracle; hardened under both sanitizer binaries.
(commit 646f7f5)

## Wave 47: BNCS SID_CHANGEPASSWORD (0x31, OLS) — password rotation
on(ChangePasswordRequest) was a stub. Wired to the unit-tested
ChangePasswordUseCase (session-hash arm): decode ticks/sessionkey/old-hash2/
new-hash1 -> execute (re-derives old hash2 from stored hash1 + ticks/sessionkey,
rotates on match) -> reply SERVER_CHANGEPASSACK. Fixed a stale forward-decl
(change_password field was typed `ChangePassword`, the real class is
`ChangePasswordUseCase`) + wired it in main with the login session hasher.
NO oracle differential: the original gates change-password on a random
per-connection sessionkey (anti-tamper) carried by OLS-session internals v3
doesn't model. Rotation verified by the use-case unit tests + ack=1 end-to-end;
331 bnet/auth unit tests pass under ASan. (commit 9312f8a)

## Wave 48: FIX BNCS session not detached on disconnect (re-login bug)
The W47 note turned out to be a real availability bug: an OLS account could not
re-login after disconnect (LOGONRESPONSE2 rc=0x02 forever) because the close
handler read the account id from the connection-level FSM (0 for the OLS path,
since login runs through the BnetFsm), so the acct_id!=0 guard skipped LogoutUser
and the session was never detached. Fix: BnetFsm::account_id() accessor; the close
handler prefers it (falls back to the connection FSM). LogoutUser then detaches
(it already keyed detach by session_id == sid). Repro login->disconnect->re-login
went rc=2 -> rc=0. Full unit suite green. (commit fca6810)

## Post-W48 full regression sweep: 31/31 diff scenarios PASS
Re-ran the entire suite (13 BNCS + userdata + profile + 15 WOL + diff_all_clients)
after the W48 close-path change (LogoutUser now runs on BNCS disconnect): 31/31
PASS, 0 regressions — incl. diff_leave/diff_chat which exercise the close path.

## Wave 49: kick-old-login (concurrent same-account login)
Lifecycle bug-hunt found: a concurrent login of an already-online account was
REJECTED by v3 (rc=0x02), but the original defaults to kick_old_login=true (drops
the old connection, lets the new in). Fix: LoginUser detaches the existing session
+ reports it via LoginResponse::kicked_session (both arms); new
IMessageRouter::disconnect (default no-op; MessageRouterImpl closes the egress);
on(LogonResponse2) closes the kicked connection. diff_concurrent_login.py: oracle
& v3 both rc=0 now. Updated the 2 duplicate-session unit tests to assert kick-old.
(commit 8dd8187)

## Wave 50: kick-old-login for W3/NLS + ignored-attach-failure fix
on(LogonProofW3Request) attached the session with `(void)attach()` — ignoring the
failure — so a concurrent W3 login left the OLD session attached and the NEW one
UNREGISTERED (ghost), with no kick. Fix: mirror w49 — detach the existing session
+ close the old connection via the router, then attach the new.
diff_concurrent_login_w3.py (observable: old connection dropped; W3 proof reply is
"success" either way): both logins succeed + old kicked, matching the oracle.
(commit a6a01bb) The lifecycle bug-hunt (drive 2 connections + diff) found w48/49/50.

## Post-W50 full regression sweep: 33/33 diff scenarios PASS
Re-ran the whole suite (incl. diff_concurrent_login + diff_concurrent_login_w3)
after the W49/W50 kick-old-login changes: 33/33 PASS, 0 regressions. kick-old-login
is complete for both OLS and W3/NLS and non-regressive.

## Wave 51: fix WOL channel/game ghost on disconnect
A WOL client that disconnected stayed in the channel roster (and any game) as a
ghost — WolFsm::on_close cleaned the session/peer/flags stores but never left the
channel/game (unlike the BNCS on_disconnect -> LeaveChannel). diff_wol_disconnect
.py: #wdisc count stayed 2 on v3 vs 1 on the oracle. Fix: on_close runs
LeaveChannel (membership removal + IRC PART to remaining members) + new
IWolGameStore::remove_player (erases an emptied game). Threaded LeaveChannel into
WolFsm/make_wol_session/main. Now matches the oracle (2 -> 1). (commit 6585c2a)
This is the 4th consecutive lifecycle bug fix (w48 re-login, w49/50 kick-old,
w51 WOL disconnect ghost) — all found by the connection-lifecycle bug-hunt.

## RUNNING TOTAL: ~84 distinct bugs/features across 51 waves. Login (all families)
## + NLS passchange + friends + game advertise/list + all chat commands + the WOL
## command surface — login, lobby LIST/JOIN, cross-session chat, the full
## JOINGAME/GAMEOPT/STARTG game lobby, FINDUSER, buddy list, codepage/locale,
## GETINSIDER, PAGE, CHANCHK, HOST, USERIP, INVMSG, SETOPT, ADVERTR — all match
## the oracle & are runtime-hardened (ASan+UBSan clean, leak-clean; 15 WOL
## differentials). The differentially-verifiable WOL command surface is COMPLETE.
## Remaining WOL items are NOT amenable to the differential harness without heavy
## setup or are large subsystems: SQUADINFO/CLANBYNAME (clan — needs clan
## membership to test beyond the degenerate no-clan case), ladder LISTSEARCH/
## RUNGSEARCH/HIGHSCORE (needs a ladder backend with data), matchbot/anongame
## automatch (~615-line subsystem). See findings/wol-chat-lobby.md.

## Wave 52: fix BNCS advertised-game ghost on host disconnect
A BNCS host that advertised via SID_STARTADVEX3 and then dropped the socket
(no SID_CLOSEGAME) left the game in the shared repo forever — a ghost in every
other client's GETADVLISTEX. diff_game_disconnect.py: after the host's raw close
the oracle returns [] but v3 still listed 'GhostGame'. Two root causes:
  (1) BnetFsm::on_disconnect left the channel but never removed the host's game.
  (2) use_cases_.leave_game was DECLARED but never wired in main.cpp -> null, so
      even explicit SID_CLOSEGAME was a silent no-op (latent, no diff covered it).
Fix: wire LeaveGame in main.cpp (shares game_repo with StartGame); on_disconnect
now runs leave_game for current_game_id_ (removes the game when its last player
leaves, mirroring on(CloseGame)). Now matches the oracle ([] after disconnect).
(5th consecutive lifecycle bug; fixes explicit CLOSEGAME teardown as a bonus.)

## Wave 53: WOL kick-old-login (second login kicks the first session)
WOL analog of w49/w50. WolFsm auth did `(void)session_registry->attach(...)` and
ignored the single-session-policy failure, so a second login for the same account
left BOTH sessions alive — the old one a ghost the oracle would have kicked.
Probe (now diff_wol_concurrent_login.py): after alice's 2nd login the oracle had
closed alice1 (a1_alive=False) but v3 kept it alive (True). Fix: mirror the BNCS
W3 kick path in wol_auth.cpp — if session_for(acct) is a different live session,
detach it + message_router_->disconnect it, then attach the new one. Now matches
the oracle (old session kicked). Also added diff_kick_channel.py: confirms a
KICKED BNCS session leaves its channel (other members see EID_LEAVE) — the kick
cascade (router disconnect -> close -> on_disconnect -> LeaveChannel) works; no
bug, regression guard only.
6th consecutive lifecycle bug fix (w48-53, minus the no-bug kick_channel probe).

## Wave 54: /squelch must be per-connection (cleared on disconnect)
The original's ignore list lives on the connection (conn_destroy frees it), so a
squelch must not survive a disconnect/reconnect — a fresh connection starts with
an empty ignore list. v3's InMemoryIgnoreStore is account-keyed + run-loop-scoped
and had no disconnect cleanup, so a squelch persisted across reconnect.
diff_squelch_reconnect.py: after alice reconnects, the oracle delivers bob's
message again (squelch cleared) but v3 still suppressed it. Fix: add
IIgnoreStore::clear_owner (default no-op; InMemory erases the owner's set) and
call it from BnetFsm::on_disconnect. kick-old (w49/50/53) guarantees a single
live session per account, so clearing on disconnect is safe. Now matches the
oracle. 7th lifecycle/cleanup fix in the w48-54 run.

## Wave 55: READUSERDATA edge cases — nonexistent fallback + BNET\ system fields
Fleet probe (agent) of the BNCS profile/userdata surface found two divergences in
SID_READUSERDATA vs the oracle's _client_statsreq:
  D1. Nonexistent-account read must fall back to the CALLER's own profile
      (`if (!reqacc) reqacc = myacc;`). v3 returned empty for an unknown name.
  D2. Auto-populated BNET\acct\* fields (username/userid, seeded at account
      creation in account.cpp) were missing — a self-read of BNET\acct\username
      returned '' on v3 vs 'bob' on the oracle.
Fix (fsm_chat.cpp on(UserDataReadRequest)): resolve the requested name to an
account; if it doesn't resolve, substitute current_username_ (and treat as self
for the BNET\-hide rule). Serve BNET\acct\username / BNET\acct\userid from the
resolved account aggregate (static, diffable; dynamic fields like ctime are
wall-clock and intentionally not served). diff_userdata_edges.py covers both.
Two probes the fleet ran came back CLEAN (already hardened): WOL user-flags
(findme/pageme) and peer-address store both cleared on WolFsm::on_close (w51) —
no ghost across reconnect. Confirms the WOL connection-scoped cleanup is complete.

## Wave 56: friend presence watch (login/logout whisper to mutual friends)
Fleet probe (agent 4) found a whole missing subsystem: the original's
WatchComponent::dispatch_whisper notifies a logging-in/out user's MUTUAL, online
friends via an EID_WHISPER (0x04) chat event — "Your friend X has entered/left
<server>." — fired from conn_set_account (login) and conn_destroy (logout),
gated on friend_get_mutual. v3 had NO presence push (friends list was query-only).
Fix: BnetFsm::notify_friends_presence(entered) — resolves our friend list via the
list_friends use-case, and for each MUTUAL (friend also lists us) + ONLINE friend,
whispers them the entered/left text. Triggered after login (OLS line 208, W3 line
306 in fsm_auth) and at the top of on_disconnect. server_name plumbed through
BnetUseCaseContext (cfg.server_name) for the message tail. diff_friends_watch.py
compares the structural part (eid + originating user + "has entered"/"has left"
prefix), normalizing the per-server server-name tail (oracle "PvPGN Realm" vs v3
"pvpgn.v3"). Now matches the oracle on both login and logout. diff_friends.py
(static add/list/remove) still passes — no regression.

## Wave 57: /whoami + scoped-deferred findings (fleet round 2)
Fleet round 2 (3 parallel agents) found three divergences:
  B. /whoami unimplemented in v3 (fell through to "Unknown command"); oracle
     reports the caller's location. FIXED: BnetFsm::handle_whoami() (self-location
     report, "You are using Battle.net and are currently in channel ...", mirrors
     the original's _handle_whoami_command -> do_whois(self)). diff_whoami.py
     (decisive observable = reply KIND is EID_INFO, not unknown-command; the
     localized text is charset-garbled in the harness).
  A. Channel OPERATOR flags — DEFERRED (subsystem). Oracle marks the first user
     of a NON-permanent channel as operator (MF_GAVEL 0x02) in EID_SHOWUSER/JOIN/
     USERFLAGS; v3 hardcodes flags=0 everywhere. A faithful fix needs: operator
     state in the Channel aggregate (members_ is an unordered_map — no join order),
     op-migration on the operator leaving, AND correct handling of permanent/
     predefined channels (v3 creates default channels with empty/non-permanent
     flags, so a naive "first user => op" would WRONGLY op the default channel's
     first user, a NEW divergence). Plus EID_USERFLAGS update broadcasts. Root
     cause: fsm_chat.cpp SHOWUSER/JOIN/USERFLAGS ChatEvent flags arg = 0.
  C. WOL friend presence — DEFERRED (cross-protocol encoding). w56 presence is
     BNCS-only (notify_friends_presence lives in BnetFsm). A WOL login/logout
     doesn't notify friends. The WOL->BNCS-recipient case would work with BNCS
     encoding, but a correct general fix needs PER-RECIPIENT protocol-aware
     encoding (a BNCS-encoded ChatEvent sent to a WOL recipient would corrupt its
     stream), which v3's router (raw-bytes broadcast) can't do today. Needs a
     protocol-aware presence-delivery mechanism (shared use-case + per-session
     protocol tag). Root cause: wol_auth.cpp/wol_fsm.cpp never call any presence
     path; router->broadcast is byte-oriented, not protocol-aware.
Two fleet probes confirmed CLEAN earlier (user-flags, peer-address). 57 waves.

## Wave 58: channel operator flag (first user of a non-permanent channel = op)
Promoted the wave-57 DEFERRED item A to a fix after confirming it is safe:
BnetdService seeds the default channels as Permanent (so they are NOT auto-op'd,
matching the oracle), user-created channels get first-user-op (matching the
oracle), and no diff inspects the chat-event flags VALUE (diff_chat only checks
EID_USERFLAGS presence). Implementation: Channel aggregate gains operator_id_
(set on first admit of a non-permanent channel; migrated to a remaining member on
the operator's leave/kick; cleared when empty) + operator_id() accessor. The JOIN
handler in fsm_chat.cpp now emits MF_GAVEL (0x02) in the per-member USERFLAGS/
SHOWUSER events and the joiner's EID_JOIN, computed from channel.operator_id().
diff_channel_op.py: bob sees the channel creator alice as operator (0x02) and
himself as 0, matching the oracle (masking the oracle's transient MF_PLUG 0x10
UDP-capability bit, which v3 does not model — a separate minor gap). All 201
channel/chat/fsm/join/leave unit tests pass; no diff regression.
Remaining deferred: WOL friend presence (C — cross-protocol encoding), full
member-flags parity (admin/op/voice tiers, MF_PLUG UDP bit, recipient-relative
squelch 0x20 in dstflags), /whois last-seen timestamp for offline bnet users.

## Wave 59: JOINCHANNEL edges — canonical name echo + re-join no-op (fleet round 3)
Fleet round 3 (3 agents) on operator-commands / account-validation / join-edges:
  FIXED (this wave): two clean JOINCHANNEL divergences —
  - EID_CHANNEL echoed the raw typed channel name; the original echoes the
    CANONICAL stored name (creator's spelling). Joining "mychan" of a channel
    created as "MyChan" now reports "MyChan". Fix: fsm_chat.cpp EID_CHANNEL text =
    join_result.value().channel.name() (was m.channel).
  - Re-joining the channel you are already in re-emitted the whole roster; the
    original's conn_set_channel no-ops that ("channel == oldchannel"). Fix: capture
    previous_channel_id before the join and early-return when it equals the joined
    channel id (suppress roster/EID_CHANNEL/JOIN). diff_channel_join_edges.py.
  NO-BUG: account-name validation — accept/reject is byte-identical to the oracle
    across 35+ names (length 2..15, symbols -_[], / \ rejected, control/high-byte/
    unicode rejected, dup rejected). Only cosmetic difference: a >32-char name —
    oracle silently drops (wire field cap UNCHECKED_NAME_STR=32, no reply), v3
    sends a graceful NO. Not a hardening gap; v3 is never more permissive.
  DEFERRED: channel operator commands /kick /ban /unban — unimplemented in v3
    (fall through to "Unknown command"). Dead code exists (kick_from_channel.cpp,
    ban_from_channel.cpp) but is unwired, routes no events, and gates on the wrong
    authorization ("operator" group vs the oracle's split: /kick allows tmpOP,
    /ban+/unban require account-admin). Subsystem — see findings.

## Wave 60: channel operator command /kick (+ /ban /unban refusal)
Promoted the wave-59 deferred operator-commands finding to a fix for /kick (the
cleanly-implementable, highest-value one, building on wave-58 operator tracking).
Implemented BnetFsm::handle_kick: caller must be the channel operator
(channel.operator_id() == self, from wave 58); resolves the target, removes it via
the LeaveChannel use-case, broadcasts EID_LEAVE (username=target) to the remaining
members, and notifies the kicked target on its own session. A non-operator's /kick
is refused with EID_ERROR (target stays). /ban and /unban are implemented as
authorization-refused (EID_ERROR): the original requires account-level admin (a
tmpOP is not sufficient) and v3 has no admin-account model, so they always refuse —
matching the oracle's refusal for the only role v3 models. This removes the prior
"Unknown command" (EID_INFO) divergence for all three. diff_channel_kick.py:
operator-kick removes the member (others see EID_LEAVE), non-op kick refused —
matches the oracle. The oracle's extra cosmetic events (operator EID_INFO ack,
victim EID_CHANNEL move) are not replicated; the decisive membership behaviour
matches. 201/201 channel/chat/fsm unit tests pass; no diff regression.
Remaining deferred: WOL cross-protocol presence; full admin-account model (real
/ban with banlist + rejoin block for true admins); full member-flags parity.

## Wave 61: hardening — malformed-input robustness confirmed + regression guard
Autonomous hardening round: 2 fleet agents fuzzed the BNCS and WOL listeners with
~100 hostile/malformed cases each (truncated headers, lying/zero/oversized length
prefixes, count-overflow array fields 0xFFFFFFFF, unknown opcodes, empty bodies
for valid opcodes, unterminated cstrings, 4MB bodies, 10k-packet floods, 200
half-open connections, over-long IRC lines, no-CRLF, NUL/binary/format-string
payloads, missing params, pre-login/out-of-order commands). RESULT: v3 has ZERO
crashes and ZERO hangs — it caps buffers like the oracle (drops over-long lines),
survives the count-overflow allocation-bait vectors, and stays responsive under
floods. The hardening directive is substantially met on the malformed-input axis.
Added diff_robustness.py (a fleet-derived battery + post-battery liveness check)
as a permanent regression guard; no code change needed. Documented behavioral
(non-fatal) DoS-resistance divergences in findings/malformed-input-safety.md.

## Wave 62: dead-code audit + remove superseded leftovers
Two fleet agents audited the whole tree for genuinely-unreferenced code (grep
evidence of zero references). Removed the unambiguous superseded leftovers (zero
build footprint, confirmed by "ninja: no work to do" + 3199/3199 unit tests):
  - protocol/bnet codec_extended.{cpp,hpp} (duplicate codec, never compiled,
    included nowhere; live codec is codec.cpp).
  - app/bnetd bnet_session_factory.hpp (never included; superseded by
    BnetBnftpDispatchFactory).
The rest is catalogued in findings/dead-code-audit.md and deliberately NOT purged:
it is dominated by intentional unwired-but-tested application-layer API and
per-context scaffolding (NOT rot), plus duplicate/superseded subsystems
(event_dispatcher, irc bridge_fsm, wolgameres, infra/plugin vs scripting/plugin,
infra/scripting/lua vs infra/lua, several built-but-unlinked infra libs, core
legacy modules) where the "which is canonical" call needs confirmation against the
composition roots — best done in a dedicated cleanup pass with build+test per
batch. The codebase is largely clean (-Werror, heavily tested).

## Wave 67: fix BNFTP body-drop (graceful close-after-flush in TcpSession)
NEW client coverage: a fleet agent wrote a BNFTP mock client (init byte 0x02 +
CLIENT_FILE_REQ) and found a SERIOUS v3 bug — BNFTP served the reply header but
ZERO file body for every file (downloads completely broken). Root cause:
BnftpFsm::try_dispatch calls ctx_->close() synchronously right after queueing the
header+body writes; TcpSession::close()/send() both post onto the strand FIFO, so
close() shut the socket down before the body write dispatched. Fix: graceful close
in TcpSession — close() now defers when writes are pending (sets close_after_flush_)
and the write-completion handler runs deliver_close once the queue drains; the idle
timer remains the stall backstop. This is a general transport improvement (every
protocol that closes after a final write now flushes first). diff_bnftp.py: v3 now
delivers the full body for sizes 1..65536, matching the oracle byte-for-byte.
Full unit suite 3199/3199; the transport change is exercised by the kick/concurrent
-login/disconnect diffs in the sweep.

## Wave 68: WOL SQUADINFO/CLANBYNAME error replies (461/439)
Fleet WOL-verb coverage diff found SQUADINFO + CLANBYNAME silently no-op'd in v3
(listed in wol_known[]) where the oracle returns 461 ERR_NEEDMOREPARAMS (no param)
and 439 ERR_IDNOEXIST (param given but the freshly-created account has no clan).
Implemented the 461/439 replies in dispatch_line (v3 has no clan backend, so the
lookup path always reports "no clan", matching the oracle's behavior for the only
role v3 models). diff_wol_squadinfo.py now passes. The ladder verbs LISTSEARCH/
RUNGSEARCH/HIGHSCORE + GAMERES (a separate binary listener, port 4807) need a
stats/ladder backend and are not cleanly diffable in this harness.

## Wave 69: implement WOL NAMES <channel> (353 roster + 366)
Following the BNFTP pattern (probe an untested verb -> find a gap), the WOL NAMES
command was a silent no-op in v3 (wol_known[]) where the oracle replies 353
RPL_NAMREPLY (channel roster, operator prefixed with '@') + 366 RPL_ENDOFNAMES.
Implemented WolFsm::on_names: resolves the channel via channel_reader_, lists
members by nick (auth_.account_reader->find_by_id) with '@' on the operator
(channel.operator_id() from wave 58), sends 353 + 366. Also fixes the latent
on_join roster which used numeric account-IDs as placeholders — NAMES now resolves
real nicks. diff_wol_names.py uses ORDERED joins (operator assignment is otherwise
a probe race: the oracle's run loop is single-threaded, v3's joins are async per-
connection) and compares the member SET + operator marking (353 order is
unordered-map-dependent). Matches the oracle. 3199/3199 units.
Note: bare NAMES (no channel) lists every channel on the oracle — config-dependent,
not differentially meaningful — so v3 returns just the 366 terminator.

## Wave 70: BNCS GETFILETIME/GETICONDATA replies + WOL TIME/MODE
Fleet round (2 agents, untested-opcode sweep) found several silent-no-op gaps; the
backend-free, cleanly-diffable ones are fixed here:
  BNCS 0x33 SID_GETFILETIME + 0x2D SID_GETICONDATA: the original always replies
    (SERVER_FILEINFOREPLY echoing type/unknown2/filename + mtime; SERVER_ICONREPLY
    with icons.bni). v3's on(FileInfoRequest)/on(IconRequest) were no-op stubs.
    The reply encoders already existed — just wired the FSM handlers to send
    FileInfoReply{echoed fields, timestamp 0 placeholder} / IconReply{0,
    "icons.bni"}. diff_fileinfo.py (compares echoed type/unknown2/filename, not the
    host-dependent timestamp). Real SC/D2 clients expect these in the TOS/icon
    handshake.
  WOL TIME -> 391 RPL_TIME (server + unix time); WOL MODE #chan -> 324
    RPL_CHANNELMODEIS "+tns" / "MODE #chan b" -> 368 end-of-ban / "MODE <nick>"
    -> 501 ERR_UMODEUNKNOWNFLAG. Were silent no-ops in wol_known[]. Implemented
    on_time + on_mode (query only; mode CHANGES route through operator commands,
    deferred). diff_wol_timemode.py.
Deferred (more involved): WOL TOPIC (needs channel topic wired — domain Channel
HAS topic_/set_topic + there's an unwired SetChannelTopic use-case; also note the
ORACLE CRASHES on a bare "TOPIC #chan" query — NULL deref, so never test that) and
WOL KICK (IRC form: operator check + KICK broadcast + member removal). BNCS
clan/realm/ladder/MOTD/ad opcodes need backends — not cleanly diffable.

## Wave 71: implement WOL KICK (operator removes a member)
WOL KICK was a silent no-op (wol_known[]) where the original lets a channel
operator kick a member (broadcasts a KICK line to the channel + removes the
victim). Implemented WolFsm::on_kick: parse #chan + victim + reason (default
"Bye"); 461 on too few params; operator-gated via channel.operator_id() (wave 58)
-> 482 ERR_CHANOPRIVSNEEDED for non-ops; 441 if the victim isn't on the channel;
capture all member sessions, remove the victim via LeaveChannel, broadcast the
KICK via route_irc_line. The KICK prefix is environment-dependent (oracle WCHT@ip
vs v3 @Battle.net) so diff_wol_kick.py compares the decisive observables: victim
receives a KICK naming them, operator sees it, victim removed from the roster
(ordered joins -> deterministic operator). Matches the oracle. 3199/3199 units.
Remaining WOL gap: TOPIC (needs the channel topic wired through a use-case; the
domain Channel has topic_/set_topic; CAUTION the oracle CRASHES on a bare TOPIC
query). BNCS clan/realm/ladder/MOTD/ad need backends — not cleanly diffable.

## Wave 72: implement WOL TOPIC (set/persist + 332 on join) — WOL verb surface complete
The last untested WOL verb. TOPIC was a silent no-op (wol_known[]). The original
stores a channel topic on "TOPIC #chan :text", echoes 332 RPL_TOPIC to the setter,
and includes 332 in every later JOIN. Implemented:
  - Wired the (previously unwired-but-tested) SetChannelTopic use-case through
    main.cpp -> make_wol_session -> WolFsm::set_channel_topic_use_case (shares the
    channel repo + router; member-gated, <=255 chars).
  - WolFsm::on_topic: SET persists via the use-case + echoes 332; bare QUERY
    replies 332 with the stored topic SAFELY (the ORACLE CRASHES on a bare query —
    NULL deref — so the test never sends one, but v3 must not crash).
  - on_join now emits 332 (the channel topic, empty when unset) like the original,
    so a later joiner sees a topic an earlier member set.
diff_wol_topic.py verifies end-to-end: A sets topic -> A gets 332 echo -> B joins
-> B sees the persisted topic. Matches the oracle. 232/232 WOL/chat/channel/join
unit tests pass (join-332 change non-regressive); 3199/3199 overall.
The cleanly-diffable WOL verb surface is now COMPLETE (waves 67-72: BNFTP,
SQUADINFO, NAMES, fileinfo, TIME, MODE, KICK, TOPIC). Remaining WOL gaps all need
backends (ladder LISTSEARCH/RUNGSEARCH/HIGHSCORE, GAMERES binary listener) or are
config-dependent — not cleanly diffable.

## Wave 73: hide "the Void" from SID_CHANNELLIST (channel-list faithfulness)
First wave to exercise SID_CHANNELLIST (0x0B / CLIENT_PROGIDENT2). The handler
(BnetFsm::on(ChannelListRequest), fsm_chat.cpp) was already wired but UNTESTED.
Probing it revealed a divergence: v3 seeded "The Void" as an ordinary permanent
channel and advertised it in the list, whereas the original flags the
kicked/banned limbo channel (channel_flags_thevoid, set when a channel's
shortname == "THE VOID") and NEVER lists it — in SID_CHANNELLIST, the /channels
command, and IRC/WOL LIST alike.

Fix (faithful, robust through persistence):
  - domain ChannelFlag gains TheVoid=8 (bitset widened 8->16 bits).
  - SqlChannelRepository save/load now round-trips all 16 flag bits (uint16
    column value), so The Void stays hidden after a restart with the SQL backend.
  - BnetdService seeds "The Void" with ChannelFlag::TheVoid set.
  - ListChannels skips TheVoid-flagged channels (covers BNCS CHANNELLIST + WOL
    LIST, matching the original's blanket exclusion).

diff_channellist.py verifies vs the oracle: both answer 0x0B with a well-formed
NUL-terminated list + empty terminator (no trailing garbage), and NEITHER lists
"The Void" (names otherwise legitimately differ per server config). 3199/3199
unit (8 known load_anongame temp-file flakes pass -j1); channel/chat/WOL diff
regression set (channellist, channelcmds, multichannel, join_edges, leave, chat,
whoami, friends, gamelist, wol_chat, wol_lobby) all match the oracle.

## Wave 74: WOL PART faithfulness (no-op off-channel + real-channel + broadcast)
Probing the WOL PART verb (implemented but untested) found three divergences
from the oracle (_handle_part_command -> conn_part_channel, which IGNORES its
params and just parts the connection's CURRENT channel via
channel_del_connection(message_type_part)):
  1. PART while NOT on a channel: the oracle is a SILENT no-op (sends nothing);
     v3 replied 442 ERR_NOTONCHANNEL.
  2. PART <wrongname> while in #room: the oracle parts the REAL current channel
     and the PART line names #room; v3 echoed the supplied parameter (#wrongname).
  3. Multi-user: the oracle broadcasts the PART to the whole channel so other
     members see the departure; v3 only echoed to the parting user and NEVER left
     the domain channel (leaver ghosted in the roster, others saw nothing).
Fix: rewrote WolFsm::on_part to ignore params, return ok() (no reply) when not on
a channel, leave the domain channel via the LeaveChannel use-case, broadcast the
PART (v3-consistent @Battle.net hostmask) to the remaining members + echo to self,
and reset channel_/channel_id_/state_ (mirrors the disconnect-path PART already in
wol_fsm.cpp). diff_wol_part.py verifies all five observables vs the oracle
(no-reply-off-channel, real-channel echo, ignores-param, self-echo, member-sees-
part). Updated the two unit tests that asserted the old 442 behavior to assert the
silent no-op. 3199/3199 units; WOL/chat diff regression set (wol_part, wol_chat,
wol_lobby, wol_names, wol_kick, wol_disconnect, leave, chat, whisper, emote,
multichannel, channel_join_edges) all match the oracle.
The PART source hostmask is environment-dependent (oracle WCHT@ip vs v3
@Battle.net), so the diff compares decisive observables, not the literal prefix
(same approach as wave 71 KICK / wave 69 NAMES).

## Wave 75: dead-code removal — infra/health (unlinked HTTP-handler lib)
Dead-code pass (focus area). `infra/health` (HealthHandler + MetricsHandler, HTTP
handlers for /healthz, /readyz, /varz, /metrics) was a STATIC lib defined in
src/CMakeLists.txt but linked by NOTHING: zero `target_link_libraries(... infra_health)`,
zero `#include "infra/health/*.hpp"` anywhere in src/ or tests/, and the classes
HealthHandler/MetricsHandler are referenced nowhere outside their own dir (only a
doc-comment cross-mention). No test dir (tests/unit/infra/health absent). Contrast
infra/discovery (KEEP — used by services/combined + has a test) and infra/metrics,
infra/webui_json (KEEP — have tests). Removed the CMake block + src/infra/health/.
Reconfigure clean, full relink + 3199/3199 units green; diff_chat still matches the
oracle (pure build-level removal, bnetd binary unchanged — it never linked the lib).

## Wave 76: BNCS channel-talk edge cases (empty body, over-long body)
Deeper divergence in the already-tested channel-talk path (BnetFsm::on(ChatCommand),
fsm_chat.cpp). The original (handle_bnet.cpp _client_message + message.cpp
message_format) handles two malformed channel-text inputs with WELL-DEFINED,
notice-free behavior, whereas v3 invented an EID_INFO "Invalid chat message"
reply at the ChatMessage::create boundary:
  - EMPTY body: the original replaces empty text with a single space
    (message.cpp:993-994 "empty messages crash some clients, just send
    whitespace") and STILL broadcasts an EID_TALK (" ") to the rest of the
    channel; the sender gets nothing. v3 rejected it -> sent the sender
    "Invalid chat message" EID_INFO and broadcast NOTHING.
  - OVER-LONG body (> MAX_MESSAGE_LEN 255): packet_get_str_const returns NULL,
    the handler returns -1, NOTHING is sent to anyone (silent discard). v3 again
    replied "Invalid chat message" to the sender.
The original NEVER sends a synthetic invalid/failed-message notice for channel
text. Fix (localized to the BNCS protocol boundary, mirroring where the original
does it — no domain change, no cross-protocol/test churn): in on(ChatCommand)'s
regular-message path, map empty -> " " before ChatMessage::create, and on
create/post failure return core::ok() (silent discard) instead of the EID_INFO
notice; broadcast the validated ChatMessage text. The domain ChatMessage value
object stays strict (empty/over-long rejected) — the empty->space and silent-drop
transforms live at the protocol edge exactly like the original.
diff_chat_edges.py verifies vs the oracle: empty -> bob sees one EID_TALK " ",
sender silent; 300-byte -> nobody receives anything, sender silent. Both match
exactly. 3199/3199 units (8 load_anongame temp-file flakes pass -j1); chat/channel
diff regression set (chat, talk, whisper, emote, multichannel, leave, squelch,
chat_edges) all match the oracle.
NOTE: lengths 224..255 are NOT cleanly diffable — the original gates those via
flood/quota (config + rate dependent) which v3 does not implement; left as a
known quota gap (see findings/message-squelch-quota.md). The WOL/IRC chat paths
still use ChatMessage::create directly (reject empty); not exercised here.

## Wave 77 (LANDED) — WOL 461 ERR_NEEDMOREPARAMS wire-format fix
HARDENING/divergence: a logged-in WOL client sending a parameter-requiring
command with NO params got a MALFORMED 461 from v3. `WolFsm::send_numeric()`
unconditionally injects " :" before its `text` arg, and every 461 call site
passed "<CMD> :Not enough parameters" *as* that text, producing a stray colon:
  v3:     :pvpgn.v3 461 alice :JOIN :Not enough parameters   (WRONG)
  oracle: :host     461 alice JOIN :Not enough parameters
The extra colon collapses "<CMD> :Not enough parameters" into a single trailing
parameter, so a real client sees the command token glued into the message text.
Fix: added `WolFsm::send_needmoreparams(cmd)` that puts the command name in the
middle-parameter (target) field — matching the codebase's existing convention
(e.g. 353 puts middle params in `target`). Replaced all ~22 461 call sites across
wol_fsm.cpp / wol_auth.cpp / wol_chat.cpp with it. Pure wire-format correction;
no behavioral/state change. New guard tests/diff/diff_wol_needmoreparams.py
(JOIN/MODE/TOPIC/KICK/GAMEOPT/STARTG/PAGE/ADDBUDDY/DELBUDDY/GETINSIDER/ADVERTR/
FINDUSER/FINDUSEREX) — all 13 match the oracle byte-for-byte (server-name
stripped). 3199/3199 units green; WOL diff regression set (login/squadinfo/chat/
kick/topic/part) + robustness battery all still match.
Remaining 461-adjacent divergences left as-is (different code paths, deferred):
PRIVMSG no-param -> oracle 461 vs v3 411 ERR_NORECIPIENT; NOTICE/WHO/NAMES verb
surface differs (documented in findings/malformed-input-safety.md).

## Wave 78 (LANDED) — BNFTP resume reply `filelen` = full file size
DIVERGENCE (on-wire value): a BNFTP CLIENT_FILE_REQ with a non-zero startoffset
(download resume) got a SERVER_FILE_REPLY whose `filelen` field disagreed with the
oracle. The original pvpgn (src/bnetd/file.cpp file_send) sets reply.filelen to the
FULL file size from stat() and only THEN fseeks to startoffset, streaming
`filelen - startoffset` bytes; past-EOF (startoffset >= filelen) it "keeps the real
filesize" and streams nothing. v3's BnftpFsm::handle_file_request instead put
`file_size - start_offset` into reply.filelen, so a resume advertised a too-small
total and a past-EOF request advertised 0 — both wrong on the wire. (Flagged as the
"Secondary note" in findings/bnftp-file.md.)
Fix (src/protocol/file/src/bnftp_fsm.cpp): reply header now carries `full_len`
(= file_size) while the streamed payload stays `file_size - clamped_start_offset`.
Pure header-value correction; byte count delivered is unchanged.
Updated the two unit tests that pinned the old (wrong) behavior
(tests/unit/protocol/file/bnftp_fsm_test.cpp): start_offset>0 and past-EOF now
expect filelen == full content size. New guard tests/diff/diff_bnftp_resume.py
covers offsets 0 / mid-file / exact-EOF / past-EOF across two file sizes — all match
the oracle (filelen + delivered bytes). 3199/3199 units green; existing diff_bnftp.py
(offset 0, sizes 1..65536) still matches.

## Wave 79 (LANDED) — WOL LIST channel names: '#' prefix + irc_convert_channel escaping
DIVERGENCE (on-wire token): a logged-in WOL client's `LIST` got 327 RPL_CHANNEL
lines whose channel name was the BARE store name — no leading '#' and with embedded
spaces left intact. The original (src/bnetd/handle_wol.cpp _handle_list_command ->
irc.cpp irc_convert_channel) prepends '#' and escapes the name (space -> '_', plus
the %-escapes for `_ % \b \n \r : ,`) so each 327 token is a single, valid IRC
channel name. v3 emitted e.g. `327 alfa ListCh 1 0 388` and `327 alfa Diablo II ...`
(the space splits the token), where the oracle sends `327 alfa #ListCh 1 0:` /
`#Diablo_II-1 ...`. v3 was also internally inconsistent: JOIN/NAMES/PART all keep
the '#', only on_list dropped it.
Fix (src/protocol/wol/src/wol_fsm/wol_chat.cpp on_list): added an irc_channel_name
lambda that prepends '#' and applies the same escaping as the original
irc_convert_channel, and routed emit_channel's name through it (tolerant of an
already-'#'-prefixed fallback name). Pure wire-format correction; channel set /
counts / official flag unchanged. New guard tests/diff/diff_wol_list.py joins a
user channel and verifies the 321 + '#'-prefixed 327 entry (count 1, official 0) +
323 envelope vs the oracle — match. The default/permanent channel SET and the WOLv1
':' vs WOLv2 ' 388' terminator are config/environment-dependent, so the test
compares the decisive user-channel observable, not the literal default list.
3199/3199 units green; WOL diff regression set (wol_list, wol_names, wol_chat,
wol_lobby, wol_part, wol_topic, wol_kick, wol_login) all match the oracle.
Deferred (not cleanly diffable): v3 always emits official flag 0 — the original
marks channel_flags_permanent channels as 1, but the permanent channel set differs
between servers and v3's ChannelInfo carries no permanent flag, so this is not
directly comparable. The original also appends a games list to LIST (numparams==0);
v3 lists only channels (no WOL game list yet).

## Wave 80 (LANDED) — SID_STARTADVEX3 parts the chat channel when advertising a game
DIVERGENCE (channel-membership ghost). The original (`_client_startgame4`,
handle_bnet.cpp:4216) unconditionally parts the host from its current channel
BEFORE advertising the game:
    // Quick hack to make W3 part channels when creating a game
    if (conn_get_channel(c)) conn_part_channel(c);
so the remaining channel members receive EID_LEAVE and the host stops ghosting in
the channel roster while it hosts. v3's `BnetFsm::on(StartGame4Request)`
(SID_STARTADVEX3 / kSidStartGame4 0x1C) created/advertised the game but left the
host IN the channel — a ghost member the oracle would have removed. Discovered
while probing the kick-old-login lifecycle (the rest of which — kick closes old
BNCS/WOL/W3 conn, channel-leave-on-kick, game-removal-on-kick, friend
entered/left presence on kick, squelch clear, raw-disconnect channel+game
cleanup — all already MATCH the oracle).
Fix (src/protocol/bnet/src/fsm/fsm_game.cpp on(StartGame4Request)): after the
login-state check and before start_game, if state_ == InChat call on(LeaveChannel{})
(faithful to the original's unconditional part-before-process; on(LeaveChannel)
no-ops when not actually a member, so the later on_disconnect leave finds nothing
and there is no double broadcast). New guard tests/diff/diff_advertise_part.py:
A,B join #chan; A advertises a game; B must see EID_LEAVE for A — matches the
oracle (before: v3 emitted nothing). 3199/3199 units green (load_anongame/
icon_req_loader temp-file flakes pass -j1); diff regression set (advertise_part,
game_disconnect, gamelist, dup_create, leave, chat, talk, multichannel,
channellist, channel_join_edges, emote, whisper, concurrent_login) all match.
Note: the original also parts the channel on SID_JOINGAME (0x1D, line 3983) and
arranged-team invite (2716); v3's on(JoinGame) is still a placeholder stub
(game_id 0) and is not cleanly diffable yet — left for a later wave. STARTGAME1
(0x08) / STARTGAME3 (0x1A) do NOT part the channel in the original (verified).

## Wave 81 (LANDED) — SID_REALMLISTREQ replies (0x40 110-era + 0x34 legacy)
DIVERGENCE (missing reply). A logged-in client's realm-list query got NOTHING
from v3. The original answers BOTH variants unconditionally, even with no realms
configured (_client_realmlistreq110 / _client_realmlistreq, handle_bnet.cpp,
registered only in the logged-in handler table):
  * CLIENT_REALMLISTREQ_110 (0x40, header-only) -> SERVER_REALMLISTREPLY_110 (0x40)
  * CLIENT_REALMLISTREQ     (0x34, two u32 cookies) -> SERVER_REALMLISTREPLY (0x34)
Both replies are a reserved u32 (== 0) + u32 realm count + `count` records; with
the default config (no active realms) the body is exactly 8 bytes (00*8). v3's
on(RealmListRequest)/on(RealmListLegacyRequest) only ran the login-state gate and
returned core::ok() — sending nothing — so a D2 client would stall awaiting the
realm list.
Fix (src/protocol/bnet/src/fsm/fsm_chat.cpp): after the existing require_clan_state
gate, both handlers now ctx_->send an empty reply — RealmListReply{} (0x40) /
RealmListLegacyReply{} (0x34). The message types + encoders already existed
(messages_realm.hpp, codec_realm.cpp); only the FSM handlers were wired. v3 has
no realm subsystem, so the list is always empty (count 0) — faithful to an oracle
with no active realms (which is the default; conf/realm.conf.in seeds none).
New guard tests/diff/diff_realmlist.py: logs in, sends 0x40 then 0x34, and
compares reply presence, >=8-byte head, reserved u32 == 0, and realm count vs the
oracle — all 8 fields match. 3199/3199 units green; diff regression set
(channellist, friends, profile, whoami, userdata, chat, whisper) still matches.
Note: with realms configured the per-realm record layout (legacy 7-u32 vs 110
1-u32 + name/desc strings) would need a v3 realm subsystem to diff — not present,
so only the empty-list case is cleanly comparable. on(RealmJoinRequest) (0x3E)
still no-ops (realm session handshake needs a d2cs backend; deferred).

## Wave 82 (LANDED) — WOL SETCODEPAGE/GETCODEPAGE/SETLOCALE/GETLOCALE no-param 461
DIVERGENCE (missing reply). A logged-in WOL client that sends any of the four
codepage/locale verbs with NO parameter got NOTHING from v3. The original
(handle_wol.cpp _handle_set/get_codepage / _handle_set/get_locale) replies with
ERR_NEEDMOREPARAMS in every no-param branch:
    :<server> 461 <nick> <CMD> :Not enough parameters
v3's on_setcodepage/on_getcodepage/on_setlocale/on_getlocale instead returned
core::ok() silently — and carried a stale, factually-wrong comment ("original:
no reply without a param"; the original source contradicts it). A real client
awaiting the 461 would stall. (These four were the only param-requiring WOL
verbs NOT covered by the wave-77 461 fix / diff_wol_needmoreparams.py.)
Fix (src/protocol/wol/src/wol_fsm/wol_chat.cpp): the four empty-param branches
now `return send_needmoreparams("<CMD>")` (the wave-77 helper that puts the
command name in the middle IRC param, no stray colon). Pure missing-reply
correction; the with-param paths (329/328/310/309 echo + set/get round-trip)
are unchanged. New guard tests/diff/diff_wol_codepage_locale.py covers both the
no-param 461 (all 4) and the set-then-get round-trip (codepage 1252, locale 5)
— all match the oracle byte-for-byte (server-name stripped). 3199/3199 units
green; WOL diff regression set (needmoreparams, chat, names, topic, kick, part,
login) all still match.

## Wave 83 (LANDED) — DEAD CODE: remove unlinked infra/shadow lib
DEAD CODE (built-but-unlinked STATIC lib, same pattern as wave 75 infra/health).
src/infra/shadow/ builds pvpgn_infra_shadow (ShadowAccountRepository +
ShadowUnitOfWork + ShadowUnitOfWorkFactory — a shadow-write/dual-backend adapter
for zero-downtime migration). Proven dead: the only references anywhere are the
add_subdirectory in src/CMakeLists.txt (1473-1475); NO target links
pvpgn_infra_shadow, NO `#include "infra/shadow/*"` exists anywhere, the three
classes are referenced nowhere outside their own dir, and there is no test dir
(grep over *.cpp/*.hpp/*.h/CMakeLists.txt + tests/ all clean). The persistence
backend_registration/adapter_registry do not mention shadow either; siblings
(infra/file, sqlite, mysql, postgres) ARE linked by bnetd/migrate/tests — shadow
is the odd one out.
Fix: removed the `if(NOT TARGET pvpgn_infra_shadow) add_subdirectory(infra/shadow)
endif()` block from src/CMakeLists.txt and `git rm -r src/infra/shadow`.
Reconfigure + full relink + 3199/3199 units green; bnetd never linked it so the
binary is behaviorally unchanged — diff_chat still matches the oracle.
Still present (have consumers/tests, do NOT remove): infra/file/sqlite/mysql/
postgres (linked), infra/discovery (used by services/combined + test),
infra/metrics + infra/webui_json (tested), infra/tracing (option-gated OTLP
scaffolding). Remaining catalogued dead-code candidates (core legacy modules,
infra/crypto/{peerchat,wol_hash}, infra/metrics/server_metrics,
infra/webui/web_server, protocol/wolgameres stub) need per-symbol/lib-dependency
care — see findings/dead-code-audit.md.

## Wave 84 (LANDED) — whisper-to-self event ordering (WHISPERSENT before WHISPER)
DIVERGENCE (event ordering on a single connection). The original do_whisper
(command.cpp) sends the sender's acknowledgement FIRST
(message_type_whisperack -> EID_WHISPERSENT 0x0a), THEN the target's message
(message_type_whisper -> EID_WHISPER 0x04). It has no self-target special case,
so when a user whispers to themselves (/w <self> ...) both lines land on the same
socket and the client observes WHISPERSENT then WHISPER, in that order.
v3's BnetFsm::handle_whisper (src/protocol/bnet/src/fsm/fsm_chat.cpp) emitted them
in the REVERSE order: it broadcast EID_WHISPER to the target session, then sent
EID_WHISPERSENT to the sender. For a whisper to ANOTHER user this is invisible
(two separate connections), but a self-whisper exposed the reversed pair
(0x04 before 0x0a vs the oracle's 0x0a before 0x04).
Fix: reordered handle_whisper to ctx_->send the EID_WHISPERSENT to the sender
first, then broadcast the EID_WHISPER to the target session (faithful to the
original's whisperack-before-whisper sequence). Behaviour for the normal
two-party whisper is byte-identical (still WHISPER to target + WHISPERSENT to
sender); only the on-wire order for the same-socket self-whisper changes.
New guard tests/diff/diff_whisper_self.py: alice whispers herself and asserts the
(eid,name) sequence [(0x0a,alice),(0x04,alice)] on both servers — match (before:
v3 gave [(0x04,alice),(0x0a,alice)]). 3199/3199 units green (load_anongame/
multilocale/maplists temp-file flakes pass -j1); diff regression set (whisper,
squelch, chat, talk, emote) all still match the oracle.

## Wave 85 (LANDED) — SID_FRIENDSLIST tolerates trailing body bytes
DIVERGENCE (missing reply on a padded request). A logged-in client that sends
SID_FRIENDSLIST (0x65) with ANY trailing bytes after the (empty) body got
NOTHING from v3, while the oracle still answered with the friends list. The
original _client_friendslistreq (handle_bnet.cpp:2298) only enforces a *minimum*
size — `packet_get_size(packet) < sizeof(t_client_friendslistreq)` (the bare
header) — so it tolerates and ignores any extra bytes a client appends. v3's
decode_friendslist_request (codec/codec_friends.cpp) instead called
check_empty_body, which fails on a non-empty payload; decode_client then returned
a codec error and the FSM dropped the packet (no reply). A real client that pads
the request would never receive its friends list.
Fix: decode_friendslist_request no longer rejects a non-empty body — it discards
any trailing bytes and returns FriendsListRequest{} (faithful to the original's
minimum-size leniency). The empty-body happy path is unchanged. Found via a new
post-login malformed-field probe comparing reply (sid,len) streams vs the oracle
across ~23 truncated/oversized BNCS bodies; FRIENDSLIST-with-trailing was the one
cleanly-diffable reply divergence (the readuserdata truncation diffs route into
the non-empty-reply count path and are not as crisply comparable).
New guard tests/diff/diff_friendslist_trailing.py: empty / 2-byte-trailing /
64-byte-trailing FRIENDSLISTREQ must all yield a 0x65 reply on both servers —
match (before: v3 replied only to the empty one). 3199/3199 units green; diff
regression set (friends, friends_watch, robustness, chat, whisper) all still
match the oracle.
Note: check_empty_body still guards SID_NULL, CLOSEGAME/CLOSEGAME2 (0x02/0x1F)
and Unknown24 (0x24); those generate no reply so a trailing-byte divergence there
is not observable via the diff harness — left as-is (the oracle also uses
minimum-size checks for them, but it is not cleanly diffable without a reply).

## Wave 87 (LANDED) — implement /time chat command (two EID_INFO lines)
DIVERGENCE (missing command). v3 had no /time handler: the inline command
dispatch in BnetFsm::on(ChatCommand) (fsm_chat.cpp) covered only who/whois/
whoami/kick/ban/users/squelch/me/friends/whisper, and the generic
CommandRegistry/permission-checker use-cases are not wired into bnetd, so /time
fell through to the "Unknown command. Type /help for a list of commands."
fallback — a single EID_INFO (0x12). The original _handle_time_command
(bnetd/command.cpp) sends TWO EID_INFO lines for a Battle.net-class connection:
"Server Time: <strftime %a %b %d %H:%M:%S over gmtime>" and "Your local time:
<same, with the connection's tz bias>" (the second is gated on
conn_get_class == conn_class_bnet, always true for a BNCS client). A real client
expecting both lines saw only the bogus unknown-command line.
Found via a command-battery probe (log in, join, fire ~25 read-only verbs at both
servers, compare the EID-code structure of each reply). /time was the cleanest,
fully self-contained divergence (no backend/permission dependency): oracle 2x
0x12, v3 1x 0x12. (Side note: the oracle CRASHES on /news with no news file — a
known upstream NULL-deref class; avoided in the probe. Many other verbs route
through the unimplemented registry path and/or the oracle's command-group
permission model — those are not cleanly diffable here and are left for later.)
Fix: new BnetFsm::handle_time() (declared in fsm.hpp, dispatched on cmd=="time")
emits the two EID_INFO lines using std::time + ::gmtime_r + strftime in the same
"%a %b %d %H:%M:%S" format. v3 negotiates no per-session tz bias over this path,
so both lines render in UTC — the two distinct lines (not the localized text,
which the oracle charset-garbles in this harness) are the faithful observable.
New guard tests/diff/diff_time.py: alice joins a channel, sends /time, both
servers must emit exactly two EID_INFO events — match (before: v3 gave one).
3199/3199 units green; diff regression set (channelcmds /who+/whois, whoami)
still matches the oracle.

## Wave 86 (LANDED) — BNFTP filename cap raised 128 -> 2047 (match legacy)
DIVERGENCE (Finding 5 in findings/bnftp-file.md). v3's BnftpFsm capped the
client-supplied filename at 128 chars (is_safe_filename, bnftp_fsm.cpp:35) with a
comment falsely claiming it "matches legacy MAX_FILENAME_STR". The original caps
at MAX_FILENAME_STR = 2048 incl. NUL (field_sizes.h:55; handle_file.cpp uses
packet_get_str_const(packet, off, MAX_FILENAME_STR)). Any on-disk file whose name
was 129..2047 chars was served by the oracle (filelen + full body) but rejected by
v3 with a size-0 reply. Probe confirmed: a 150/200/250-char .bin name -> oracle
(100,100), v3 (0,0) before the fix.
Fix (src/protocol/file/src/bnftp_fsm.cpp): kMaxFilenameLen 128 -> 2047 (matches
the legacy cap minus the NUL) and rewrote the misleading comment. Path-traversal
protection (reject '/', '\\', '\0', empty) is unchanged, so the larger cap adds no
new attack surface — the limiting factor in practice is the filesystem's 255-byte
per-component limit, well under 2047. No unit test depended on the 128 value.
New guard tests/diff/diff_bnftp_longname.py: places 100/150/200/250-char files in
both servers' file dirs and asserts both serve the full body (filelen + bytes) —
all 4 match (before: only the 100-char control matched). 3199/3199 units green;
BNFTP diff regression set (bnftp, bnftp_resume, fileinfo) all still match.
Remaining BNFTP gaps (need work/scope decisions, not cleanly diffable here):
Finding 3 (REQ2/REQ3 W3 two-step), Finding 4 (localized/alias file resolution).

## Wave 88 (LANDED) — MF_PLUG (no-UDP plug) on the FIRST channel join
DIVERGENCE (channel-join userflags). The original creates every bnet connection
with MF_PLUG (0x10, "tiny plug, no UDP") set (connection.cpp:383) and sheds it on
the FIRST channel join via channel_set_userflags (handle_bnet.cpp:3704). What
OTHER channel members observe for a freshly-connected normal (non-op) user is:
first join -> EID_JOIN(flags=0x10) immediately followed by TWO EID_USERFLAGS(0)
(channel_set_userflags double-broadcasts: conn_set_flags() fires
channel_update_userflags because the flags changed, then it calls
channel_update_userflags AGAIN directly — an upstream double-broadcast quirk);
every subsequent join shows EID_JOIN(flags=0) with no trailing USERFLAGS (the
plug is already gone). v3's BnetFsm::on(JoinChannel) (fsm_chat.cpp) broadcast only
EID_JOIN with flags=0 and never emitted the plug or the trailing USERFLAGS, so a
real client never saw the brief no-UDP plug icon on a user's first appearance.
Found via a channel-join userflags probe (bob watches alice's first join, carol
watches alice's second join) comparing the (eid,flags,name) stream other members
receive: oracle first=[(2,16),(9,0),(9,0)] second=[(2,0)]; v3 gave [(2,0)] for
both.
Fix: new per-FSM bool plug_active_ (fsm.hpp), true until the first real
(channel-changing) join. On(JoinChannel), when there are other members to notify,
OR 0x10 into the broadcast EID_JOIN flags while plug_active_, then broadcast two
EID_USERFLAGS(0) for the joiner (mirroring the double-broadcast). plug_active_ is
cleared after every successful real join (a same-channel re-join returns early and
does not consume the plug), so later joins broadcast flags=0 with no USERFLAGS.
The joiner's own (self) roster path is unchanged — it already diverges in flag
detail and is not what this diff measures; only the other-members' observable view
is corrected here.
New guard tests/diff/diff_join_userflags.py: bob sees alice's first join as
[(2,16),(9,0),(9,0)], carol sees the second as [(2,0)] — match (before: v3 gave
[(2,0)] for both). 3199/3199 units green; diff regression set (chat, multichannel,
channel_op, kick_channel, channel_kick, leave, channel_join_edges, talk, emote,
channelcmds, whoami, squelch) all still match the oracle.

## Wave 90 (LANDED) — WOL HOST/USERIP/INVMSG send 461 on missing params
DIVERGENCE (silent where oracle replies). Three WOL verbs had a `return
core::ok();  // original guard` short-circuit for the too-few-params case, so a
real client got NOTHING back where the original (handle_wol.cpp) sends the
standard ":<server> 461 <nick> <CMD> :Not enough parameters". Affected:
on_host (HOST, needs >=1 param), on_userip (USERIP, needs >=1 param), on_invmsg
(INVMSG, needs >=3 params) — all in wol_fsm/wol_chat.cpp. The earlier wave-77/82
needmoreparams work and diff_wol_needmoreparams.py covered JOIN/MODE/TOPIC/KICK/
GAMEOPT/STARTG/PAGE/ADDBUDDY/DELBUDDY/GETINSIDER/ADVERTR/FINDUSER/FINDUSEREX but
not these three. The oracle's _handle_{host,userip,invmsg}_command each fall to an
`else irc_send(conn, ERR_NEEDMOREPARAMS, "<CMD> :Not enough parameters")`.
Fix: replaced the three early `return core::ok()` with
`return send_needmoreparams("HOST"/"USERIP"/"INVMSG")`. The happy paths (host
relay, userip lookup/401, invmsg invitee relay) are untouched.
New guard tests/diff/diff_wol_needmoreparams2.py: sends HOST/USERIP bare and
INVMSG with 2 params to both servers, server-name-stripped 461 lines match
byte-for-byte (before: v3 sent nothing). 3199/3199 units green; WOL diff
regression (needmoreparams, invmsg, userip, chat, login) all still match.

## Wave 89 (LANDED) — SID_PING (CLIENT_ECHOREPLY 0x25) must NOT be echoed back
DIVERGENCE (spurious reply). The server sends SERVER_ECHOREQ (0x25ff) and the
client bounces it back as CLIENT_ECHOREPLY (SID 0x25). The original's
_client_echoreply (handle_bnet.cpp:984) uses the cookie ONLY to compute round-trip
latency (conn_set_latency) and sends nothing in response. v3's BnetFsm::on(Ping)
(fsm_misc.cpp) instead mirrored the cookie verbatim back to the client as a 0x25
packet — a phantom ECHOREQ a real client never expects. Probe: client logs in,
drains/echoes any server-initiated pings, then sends one SID_PING(0x25): oracle
replies with nothing, v3 replied with 0x25 carrying the cookie.
Fix: on(Ping) is now a pure advisory no-op (return core::ok()) in every state,
matching the original's record-latency-and-stay-silent behaviour. Updated the two
unit tests that previously asserted the (wrong) echo to assert no reply, and the
fsm_misc doc-comment. The golden_replay/codec Ping decode tests are unaffected
(they exercise decoding, not the reply).
New guard tests/diff/diff_ping_noreply.py: after a quiet login a client sends one
SID_PING and asserts zero further packets on both servers — match (before: v3 sent
one 0x25 back). 3199/3199 units green (load_anongame/multilocale temp-file flakes
pass -j1); diff regression set (chat, w3_login, ols_login, whisper,
concurrent_login) all still match the oracle.

## Wave 91: removed dead infra/webui web_server (unlinked Boost HTTP lib)
Dead-code removal. The `infra_webui` STATIC lib (src/CMakeLists.txt, the
`if(PVPGN_V3_WITH_BOOST)` block) compiled `infra/webui/src/web_server.cpp` but no
target linked it (libinfra_webui.a was built then dropped on the floor). Proven
dead: `grep -rn infra_webui` over src/tests showed only the lib definition; no
DEPS/target_link references it; `web_server.hpp`/`dashboard_html.hpp`/`WebServer`
referenced nowhere outside infra/webui/src/web_server.cpp; the only webui test
(channel_json) links the separate INTERFACE lib `infra_webui_json`. Removed the
STATIC-lib CMake block + web_server.cpp + web_server.hpp + dashboard_html.hpp.
KEPT `infra_webui_json` + channel_json.hpp (LIVE, tested). Reconfigure + full
relink + 3199/3199 (load_anongame_infos temp-file flake passes -j1);
channel_json test still 25 assertions green; diff_chat still matches the oracle
(bnetd never linked infra_webui, so the binary is behaviorally unchanged).

## Wave 92 (LANDED) — unknown/failed chat command -> EID_ERROR (not EID_INFO)
DIVERGENCE (wrong event kind). The original routes command FAILURES (unknown
command, deactivated, reserved-for-admins) through message_send_text(...,
message_type_error, ...) which message_bnet_format (message.cpp:1305) emits as
SERVER_MESSAGE_TYPE_ERROR (EID_ERROR 0x13) with flags=0 and an EMPTY username,
whereas successful command OUTPUT goes out as message_type_info (EID_INFO 0x12,
also empty username). v3's BnetFsm::on(ChatCommand) fallback (fsm_chat.cpp) had
collapsed BOTH into a single EID_INFO reply carrying username="Battle.net", so a
real client saw an unknown command ("/foobar") as an informational notice instead
of an error. Probe: `/nonexistentcmd` -> oracle (eid=0x13, username=''), v3
(eid=0x12, username='Battle.net').
Fix: added a `bool is_error` set in the three registry error branches (NotFound ->
"Unknown command", PermissionDenied, generic command-error) and in the no-registry
unknown-command else; the final send now emits EID_ERROR with an empty username
for failures and keeps EID_INFO for successful output. Updated the one unit test
that asserted the old EID_INFO for an unknown command
(fsm_channel_test.cpp R306) to assert EID_ERROR + empty username.
New guard tests/diff/diff_unknown_command.py: `/nonexistentcmd` must come back as
EID_ERROR with an empty username on both servers — match (before: v3 gave EID_INFO
/'Battle.net'). 259/259 bnet units green; full unit suite green (load_anongame/
multilocale temp-file flakes pass -j1); chat diff regression set (unknown_command,
whoami, channelcmds, whisper, emote, squelch, leave, chat_edges, channel_join_edges)
all still match the oracle. (Pre-existing: diff_talk hangs in this harness env on
BOTH the clean and patched tree — unrelated, the non-slash text path is untouched.)

## Wave 93 (LANDED) — WOL PING -> PONG wire format (server-name prefix + param)
DIVERGENCE (wrong wire format), found via a NEW WOL malformed/edge-input fuzz
sweep (truncated/extra-param/colon variants across PING/JOIN/MODE/PRIVMSG/USERIP/
KICK/TOPIC/etc). The original (irc.cpp _handle_ping_command + irc_send_pong)
answers a client PING with ":<server> PONG <server>[ :<token>]" — the server
hostname is BOTH the message source AND the first PONG parameter, and the client
token is appended as a trailing param only when supplied; a bare "PING" gets
":<server> PONG <server>" with NO trailing colon. The original also takes the
FIRST middle token for an unprefixed arg vs the whole trailing text for a
':'-prefixed arg. v3's WolFsm::on_ping (wol_fsm/wol_chat.cpp) instead emitted a
bare "PONG :<token>" — it dropped the ":<server> " source prefix, omitted the
server-name parameter entirely, and emitted a stray empty ":" for a token-less
PING. A real WOL client doing keep-alive PINGs got a malformed PONG.
Probe: bare PING -> oracle ":<host> PONG <host>", v3 "PONG :"; PING :12345 ->
oracle ":<host> PONG <host> :12345", v3 "PONG :12345".
Fix: on_ping now builds ":<server> PONG <server>" and appends " :<token>" only
when a token was parsed (trailing kept whole for a ':'-arg; first token only for
an unprefixed arg), mirroring the original exactly.
New guard tests/diff/diff_wol_ping.py: bare PING / PING :token / PING token,
server-name-normalized to <S>, all three match the oracle byte-for-byte (before:
v3 diverged on all three). 3199/3199 units green (incl. all 63 wol unit tests);
WOL diff regression (login, chat, needmoreparams, part, list, userip, names,
timemode, kick, topic) + diff_robustness all still match/survive.
Other fuzz-surfaced WOL divergences NOT fixed this wave (left for follow-up; each
is a separate behavior, not a crash): JOIN of an unprefixed name (oracle 403 vs
v3 auto-creates), MODE +unknownchar (oracle 472 vs v3 324 echo), PRIVMSG to self
(oracle echoes vs v3 401), TOPIC with extra middle param (oracle 442 vs v3 sets),
401 ERR_NOSUCHNICK stray leading ':' before the nick in USERIP/PRIVMSG paths,
LIST with extra params (oracle returns empty list). None crash or hang.

## Wave 106
SID_CHARLIST (0x37): v3's BnetFsm::on(CharListRequest) gated on login then
returned ok() with no reply, leaving a D2 client stalled. The oracle's
_client_charlistreq UNCONDITIONALLY answers SERVER_UNKNOWN_37 (0x37) with
unknown1=0, max_chars=8, count=#closed-chars (and one record each). A plain
account has no closed-character list (account_get_closed_characterlist == NULL),
so count=0 and the body is exactly 12 bytes — config/backend-independent. Fix:
after the login gate, send ServerMessage{CharListReply{}} (defaults
unknown1=0, max_chars=8, count=0, empty char_data; encoder already existed in
codec_realm.cpp). Same class as wave 81 (realmlist) and wave 97/motd_w3.
New guard tests/diff/diff_charlist.py asserts both servers reply on 0x37 with
len>=12, unknown1==0, matching count (0). diff_realmlist + diff_motd_w3 still
match; unit suite green (anongame/icon flakes pass on -j1).

## Wave 107
WOL NAMES on an unknown channel now mirrors the oracle's fallback. The original
_handle_names_command (handle_wol.cpp:629-656) resolves the requested channel and,
when it does not exist, falls back to the connection's CURRENT channel
(`if ((!channel) && (!(channel = conn_get_channel(conn)))) continue;`), replying
with that channel's roster+name; if the user is in no channel it emits NOTHING.
v3 (WolFsm::on_names, wol_chat.cpp) always built a roster from the REQUESTED name
and always emitted an empty 353+366. Fix: resolve the channel via
channel_reader_->find_by_name; on miss, substitute the current channel
(state_==InChannel && !channel_.empty()) and rebuild from it, else return
core::ok() silently. 353+366 now use the resolved channel's name in both numerics.
New guard tests/diff/diff_wol_names_unknown.py asserts Case A (fallback to current
channel name+roster, op '@'-marked) and Case B (total silence). diff_wol_names +
diff_wol_list still match; unit suite green (3199/3199).

## Wave 108 (LANDED) — DEAD CODE: remove src/core/* sublib cluster (10 unlinked Plan-02 leftovers)
Dead-code removal, same pattern as waves 75/83/91 but a whole cluster. The
src/core/{strings,encoding,types,time,net,util,debug,error,config,version}
directories were built via 10 add_subdirectory lines (src/CMakeLists.txt:225-234)
producing 10 STATIC/INTERFACE libs (core_strings .. core_version) that formed a
closed dependency sub-graph (core_config->core_util->core_net->core_strings;
core_time->core_types; etc.) with NO target outside the cluster linking them.
Proven dead: per-lib `grep core_<x>` over CMakeLists shows references only between
cluster members; the roots (core_config/core_time/core_debug/core_encoding/
core_error/core_version) have zero refs anywhere; no `v3::core_` alias use; the
dir-local headers (conf.h/hexdump.h/util_hex.h/bnettime.h/util_time.h) are
#included nowhere outside the cluster. Functionality was superseded by the `core`
umbrella lib (header-only core/bnettime.hpp, core/hexdump.hpp, core/version.hpp
used by bnetd + tests via DEPS core). Removed the 10 add_subdirectory lines + the
10 dirs. KEPT src/core/{include,src} (the LIVE umbrella `core` lib). Reconfigure +
full relink: bnetd/d2cs/d2dbs/all tests still link (none depended on the cluster)
— linker proof of deadness. 3199/3199 unit tests green; diff_chat + diff_channellist
still match the oracle (bnetd never linked these libs, so the binary is
behaviorally unchanged).

## Wave 109 (LANDED) — BNCS slash-command dispatch made case-INsensitive (oracle parity)
The oracle dispatches chat slash-commands case-insensitively: command.cpp's
strstart() compares with strncasecmp() (util.cpp), so "/TIME", "/Time", "/WHOAMI",
"/Me" all reach their handlers. v3's BnetFsm::on(ChatCommand) in fsm_chat.cpp
extracted the command token and compared it case-SENSITIVELY against the "time"/
"whoami"/"me"/... literals, and the CommandRegistry lookup (commands_.find(name))
plus the no-registry fallback ("help") were also case-sensitive — so any
non-lowercase spelling fell through to the EID_ERROR (0x13) "Unknown command"
reply instead of running the handler. Decisive observable: "/TIME" -> oracle emits
2x EID_INFO (0x12) "Server Time:"/"Your local time:"; v3 emitted 1x EID_ERROR.
Same for "/Time", "/WHOAMI". FIX: lowercase only the command-NAME portion
(arguments keep their case, matching the oracle's prefix-only strstart): in
fsm_chat.cpp build an ASCII-lowercased `cmd`/`cmd_name` before the literal
comparisons (both the intercept block and the no-registry fallback); in
command_registry.cpp lowercase the key at both register_command() and
parse_command_line() (new to_lower_ascii helper). New guard
tests/diff/diff_command_case.py asserts /time,/TIME,/Time and /whoami,/WHOAMI,
/WhoAmI all yield identical EID structure on both servers (PASS). 3199/3199 unit
tests green; diff_time / diff_unknown_command / diff_emote / diff_squelch still
match the oracle.

## Wave 111 (LANDED) — BNFTP CLIENT_FILE_REQ2 (0x0200) two-step handshake: 0xdeadbeef + keep-open
A BNFTP connection (init byte 0x02) that sends a CLIENT_FILE_REQ2 packet
(type 0x0200 — the War3 two-step download start) diverged decisively. ORACLE:
replies with a raw 4-byte SERVER_FILE_UNKNOWN1 = 0xdeadbeef (wire `ef be ad de`)
and KEEPS the connection open (enters conn_state_pending_raw to await
CLIENT_FILE_REQ3), per src/bnetd/handle_file.cpp:82-93. V3: BnftpFsm::try_dispatch
treated any pkt_type != kClientFileReq (0x0100) as unknown and closed the
connection gracefully, sending ZERO bytes. Decisive observable: oracle -> 4 bytes
0xdeadbeef, socket open; v3 -> 0 bytes, socket closed. This was Finding 3 of
bug-hunt/findings/bnftp-file.md (open after waves 67/68/78/86/102). FIX: in
src/protocol/file/src/bnftp_fsm.cpp try_dispatch, before the generic unknown-type
close, add a branch for pkt_type == kClientFileReq2 that consumes the 20-byte
packet, sends a raw 4-byte LE 0xdeadbeef via ctx_->send_bytes, and transitions
to a new State::PendingRaw WITHOUT closing (mirrors conn_state_pending_raw).
on_bytes idles in PendingRaw (keeps the socket open; the W3 REQ3 second half is
not yet served, matching the oracle for a client that never completes REQ3). Added
kClientFileReq2/kServerFileUnknown1 constants to codec.hpp and PendingRaw to the
State enum. New guard tests/diff/diff_bnftp_req2.py asserts both servers return
the 4-byte 0xdeadbeef and keep the socket open (PASS). 3199/3199 unit tests green;
diff_bnftp / diff_bnftp_missing still match the oracle.

## Wave 112 (LANDED) — EID_LEAVE (0x03) carries the departing operator's MF_GAVEL flag
When a channel's tmpOP (operator) leaves or disconnects, the oracle broadcasts
EID_LEAVE (0x03) to the remaining members with the leaving user's channel flags
in the FLAGS u32 (offset 4): MF_GAVEL=0x02 for the operator, 0x00 for a normal
member. v3 hardcoded flags=0, so remaining members saw the operator depart with
flags=0. Leftover from wave 58, which added the gavel flag to
EID_USERFLAGS/EID_SHOWUSER/EID_JOIN in fsm_chat.cpp but missed the EID_LEAVE site.
FIX: in BnetFsm::on(const LeaveChannel&), BEFORE execute() (which migrates/clears
operator_id_), query channel_reader->find_by_id and capture whether the leaver was
the operator into leaver_flags (0x02 vs 0x00); pass leaver_flags as the EID_LEAVE
ChatEvent flags. The on_disconnect path reuses on(LeaveChannel{}), so this one site
covers both SID_LEAVECHAT and disconnect-driven leaves. Mirrors the join-site flag
computation and the /kick operator gating. New guard tests/diff/diff_leave_opflags.py
asserts operator-leave=0x02 / normal-leave=0x00 against the oracle (PASS). Unit
suite green (anongame/icon temp-file flakes pass on -j1); diff_leave still matches.

## Wave 113 (LANDED) — JOINCHANNEL emits EID_CHANNEL (0x07) FIRST, before the self-roster
On SID_JOINCHANNEL into a fresh channel, the oracle (channel_add_connection,
channel.cpp:460) sends message_type_channel = EID_CHANNEL (0x07) as the VERY FIRST
CHATEVENT, before any roster (USERFLAGS/SHOWUSER) entries. v3 emitted the self-roster
EID_USERFLAGS(0x09)/EID_SHOWUSER(0x01) first and EID_CHANNEL dead last — an inverted
wire-order. Real BNCS clients treat EID_CHANNEL as the "you are now in channel X /
reset roster" signal and expect SHOWUSER entries to follow it, so v3's order was a
fidelity bug. FIX: in BnetFsm::on(const JoinChannel&) (fsm_chat.cpp), relocate the
EID_CHANNEL ctx_->send block to run BEFORE the self-roster member loop (just after the
same-channel re-join no-op guard) instead of after it. Mechanical move, no new state;
the no-use-case fallback path already sent EID_CHANNEL first. New guard
tests/diff/diff_join_channel_first_event.py asserts the first post-JOIN CHATEVENT is
0x07 on both servers (PASS). 3199/3199 unit tests green; diff_channel_join_edges and
diff_join_userflags still match the oracle.

## Wave 114 (LANDED) — COMPINFO1 (0x05) / COMPINFO2 (0x1E) now reply (were silent no-ops)
The legacy/OLS retail login flow (Starcraft/Diablo pre-NLS) opens with
CLIENT_COMPINFO1 (SID 0x05). The oracle (_client_compinfo1, handle_bnet.cpp:381)
always replies with TWO packets: SERVER_COMPREPLY (SID 0x05, 16-byte body of the
four magic registration constants reg_version=0x00000001 / reg_auth=0xaa8843d1 /
client_id=0x001b9dda / client_token=0xab69f79a) followed by SERVER_SESSIONKEY1
(SID 0x28, 4-byte session key). The sibling CLIENT_COMPINFO2 (SID 0x1E) replies
SERVER_COMPREPLY + SERVER_SESSIONKEY2 (SID 0x1D, sessionnum + sessionkey). v3
handled both as silent no-ops (BnetFsm::on returned core::ok() with no reply),
sending 0 bytes and stalling legacy clients. FIX: in fsm_auth.cpp, wire
on(CompInfo1Request) to ctx_->send(CompReply{}) (defaults already equal the four
constants) then SessionKey1, and on(CompInfo2) to CompReply{} then SessionKey2.
The session key is a deterministic always-nonzero hash of the session id
(legacy_session_key helper) — the oracle uses a random conn_get_sessionkey but
only SID/length is peer-observable. Encoders/structs/variant entries already
existed (codec_legacy_ols.cpp, messages_legacy.hpp); only handler wiring changed.
New guard tests/diff/diff_compinfo.py byte-compares the constant 16-byte COMPREPLY
and asserts SESSIONKEY1/2 SID+length against the oracle (PASS). 3199/3199 unit
tests green; diff_ols_login and diff_dup_create still match the oracle.

## Wave 116 (LANDED) — Remove dead runtime/ composition-root scaffolding (11 files)
An 11-file cluster under src/runtime/ was genuinely dead: never compiled, never
included. The runtime STATIC lib (src/CMakeLists.txt) only lists service_host.cpp,
peer_link.cpp, cli.cpp, daemonize.cpp, crash_handler.cpp (+win_service.cpp on WIN32);
there is no GLOB anywhere. Confirmed: 0 hits in build/v3-dev/compile_commands.json and
0 .o files for each of capability_token / health_check / service_config_loader /
service_logger / service_metrics. Each of the 6 headers (those 5 + composition_root.hpp)
was #included only by its own .cpp (composition_root.hpp by nothing). Two apparent
external refs were false positives: NetworkConfig resolves to
pvpgn::infra::config::NetworkConfig (server_config.hpp), and the CapabilityToken used by
peer_link_test.cpp is the DIFFERENT live struct in runtime/peer_link.hpp:63 (encode/
decode), not the dead sign/verify class — both declared pvpgn::runtime::CapabilityToken,
a latent ODR hazard now eliminated. REMOVED: src/runtime/src/{capability_token,
health_check,service_config_loader,service_logger,service_metrics}.cpp and
src/runtime/include/runtime/{capability_token,health_check,service_config_loader,
service_logger,service_metrics,composition_root}.hpp. No CMakeLists/include edits needed.
Rebuild = "ninja: no work to do" (TUs never entered any target). 3199/3199 unit tests
green; diff_chat and diff_concurrent_login still match the oracle.

## Wave 117
BNCS whisper now honors /squelch on the recipient side. The oracle's message_send
sets MF_X for any dst ignoring src and message_type_whisper returns -1, so a
squelched sender's /w is silently dropped for the recipient while the sender still
gets EID_WHISPERSENT. v3's BnetFsm::handle_whisper delivered EID_WHISPER
unconditionally. FIX (src/protocol/bnet/src/fsm/fsm_chat.cpp): after the
unconditional WHISPERSENT ack and before broadcasting EID_WHISPER, drop the
recipient delivery when use_cases_.ignore_store->ignores(target_id,
current_account_id_) — mirroring filter_squelched. Sender ack preserved; self-
whisper unaffected (no self-ignore). New guard tests/diff/diff_whisper_squelch.py:
bob /squelch alice -> alice /w bob X -> bob gets NO EID_WHISPER, alice gets
EID_WHISPERSENT, identical on both servers. 3199/3199 unit tests green; diff_whisper,
diff_whisper_self, diff_squelch still match the oracle.

## Wave 118
BNCS bad-marker packet no longer wedges the v3 framer. A single malformed-marker
packet (any leading byte != 0xFF with a sane 16-bit size field) permanently
disabled ALL further packet processing on a v3 connection while leaving the
socket open: BnetFramer::feed did a bare `break` on header-parse failure without
consuming the bytes or closing, so the junk sat at the head of the buffer forever
and the framing loop never progressed (remote, unauth-able framing-desync DoS).
The oracle's t_bnet_header is {bn_short type; bn_short size} and never validates
the 0xFF marker — a non-0xFF byte just yields an unmatched type; it frames purely
by the size field, consumes the unknown packet by its declared size (logged +
ignored), and resyncs. FIX (src/app/bnetd/src/main/bnet_framer.hpp): on
parse_bnet_header failure, peek the LE u16 size at offset 2; if size < 4 ->
truly corrupt, set wants_close (caller closes, mirroring the oracle destroying
such conns); else if fully buffered, erase `size` bytes and continue (drop +
resync); else break to await more. Wired wants_close -> session->close() in the
BNet set_on_bytes lambda (src/app/bnetd/src/main/bnet_bnftp_dispatch.cpp). New
guard tests/diff/diff_bad_marker_resync.py: post-login inject `00 25 08 00 AA AA
AA AA` then a valid SID_FRIENDSLIST — both servers still reply 0x65 to that and a
3rd packet, socket stays open. 3199/3199 unit tests green; diff_friends,
diff_channelcmds still match the oracle.

## Wave 79
CLIENT_PROGIDENT (SID 0x06) legacy/pre-NLS login (Diablo / old Starcraft / D2)
was a literal no-op on v3 (BnetFsm::on(const ProgIdent&) returned core::ok()
with no reply), so a real legacy client blocked forever waiting for the
SERVER_AUTHREQ1 versioncheck filename + CheckRevision equation it needs to
proceed — the entire old login flow was dead. The oracle's _client_progident
always answers with SERVER_AUTHREQ1 (SID 0x06: u64 timestamp + filename cstring
+ equation cstring); select_checkrevision returns a hard-coded default even
without versioncheck config, so this is NOT backend-dependent. FIX
(src/protocol/bnet/src/fsm/fsm_auth.cpp): record client identity
(client_tag_ from packed-BE clienttag, version_id_ from versionid, mirroring
conn_set_clienttag/versionid) and send an AuthReq1Server reply with the same
fixed "ver-IX86-1.mpq" + representative equation v3's on(AuthInfo) already
emits. encode(AuthReq1Server) + the ServerMessage variant entry already existed,
no codec work. New guard tests/diff/diff_progident.py asserts both servers
return a single SID 0x06 packet whose body parses as u64 + two non-empty
NUL-terminated strings (structure only; timestamp/filename/equation are
advisory). 3199/3199 unit tests green; diff_compinfo and diff_ols_login still
match the oracle.

## Wave 119
Fixed channel operator (gavel) wrongly migrated to a remaining member when the
op leaves/disconnects/is kicked. v3's reassign_operator_on_leave (channel.hpp)
promoted members_.begin() to operator; the oracle promotes NO ONE — on member
removal it only does conn_set_tmpOP_channel(connection, NULL)
(channel.cpp:548-551), and grants tmpOP only at join when currmembers==1
(channel.cpp:464-470). FIX: reassign_operator_on_leave now only clears the gavel
iff the departing member was the operator (drop the begin() else-branch);
corrected the two misleading comments. Decisive probe: alice (tmpOP) + bob join
fresh "OpHunt", alice disconnects, carol joins and inspects SHOWUSER — oracle
bob flags=0, v3 was=2, now=0. Covers /leave, disconnect, and /kick paths (all
route through reassign_operator_on_leave). New guard tests/diff/diff_op_no_transfer.py
(masks OP bit 0x02; oracle also toggles transient MF_PLUG 0x10 v3 doesn't model).
Safe wrt wave 112 (EID_LEAVE op-flag snapshots the leaving op's flag before
removal). 3199/3199 unit tests green; diff_channel_op / diff_channel_kick /
diff_leave_opflags still match the oracle.

## Wave 120
Fixed SID_CHATEVENT (0x0F) fixed-field divergence. The oracle's
message_bnet_format (bnetd/message.cpp:1005-1007) UNCONDITIONALLY writes a fixed
wire layout into EVERY chat event: account_num (bn_int_nset
SERVER_MESSAGE_ACCOUNT_NUM=0x0df0adba) and reg_auth (bn_int_set
SERVER_MESSAGE_REG_AUTH=0xBAADF00D) both emit the identical on-wire bytes
0D F0 AD BA (LE-decoded 0xBAADF00D), and message_type_info/error use an EMPTY
username. v3 diverged on two fields: (1) EID_INFO (0x12) sent the literal
"Battle.net" username instead of "" (its own comment already noted the original
uses ""); (2) the dummy acct/reg fields carried 0xBADC0FFE for info/error and 0
for join/talk/etc, vs the oracle's 0xBAADF00D everywhere. v3's ChatEvent codec
writes both fields little-endian, so byte parity requires the LE value
0xBAADF00D in both. FIX (fsm_chat.cpp): added named constants
kChatEventAcctNum/kChatEventRegAuth (= chat::kServerMessageRegAuth) and routed
ALL 26 ChatEvent emissions (info/error/join/talk/emote/whisper/whisperack/
leave/userflags/showuser/channel/kick) through them; replaced the EID_INFO
"Battle.net" username literal with "" at every info emitter. Decisive probe:
oracle EID_INFO uname=b'' acct=reg=0xbaadf00d; v3 now identical (was
uname=b'Battle.net' acct=reg=0xbadc0ffe). New guard
tests/diff/diff_chatevent_fields.py asserts the fixed header
(flags/latency/player_ip/account_num/reg_auth) + username match byte-for-byte
for EID_INFO and EID_ERROR (text is charset-garbled, structure-only). 3199/3199
unit tests green (anongame temp-file flakes pass -j1); diff_time / diff_chat /
diff_whisper / diff_emote / diff_talk / diff_join_userflags / diff_channel_kick
still match the oracle.

## Wave 122
WOL CVERS/VERCHK now enforce strict arity (EXACTLY 2 middle params), matching
the oracle (handle_wol.cpp:688 CVERS `numparams==2`, :714 VERCHK
`numparams==2`; any other count -> 461 ERR_NEEDMOREPARAMS). Before: v3's
on_cvers/on_verchk (wol_auth.cpp) only checked for a single space (>=2 tokens)
and otherwise proceeded, so a 3-param line slipped through:
`VERCHK 1000 1.0 extra` -> v3 379 (oracle 461); `CVERS 1 1000 extra` -> v3
silent (oracle 461). FIX: parse the param string with split_irc_params() (which
excludes the trailing ':' text, mirroring the oracle's numparams) and require
middle.size()==2, else send_needmoreparams(). Exposed split_irc_params()/
IrcParams via wol_internal.hpp (moved out of wol_chat.cpp's anon namespace) so
both TUs share the one faithful middle/trailing split — `VERCHK 1000 :1.0` is
numparams==1 -> 461, which a raw whitespace token count would miss. New guard
tests/diff/diff_wol_verchk_arity.py: both conns send `CVERS 1 1000` first
(classes WOL), then probe; asserts 461 for 3-param/1-param/trailing-only cases
and 379/no-op for the 2-param normal path, all matching the oracle. 3199/3199
unit tests green; diff_wol_login / diff_wol_needmoreparams still match.

## Wave 123
Removed the dead `infra_tracing` static library (src/infra/tracing/:
log_trace_sink.cpp + otlp_trace_sink.cpp and their headers, providing
LogTraceSink/OtlpTraceSink implementations of the domain trace_sink port).
It was compiled into src/libinfra_tracing.a on every build but linked by
nothing: `infra_tracing` appeared only in its own definition block
(src/CMakeLists.txt:1630-1662) with no DEPS/target_link_libraries consumer,
the classes/headers were referenced nowhere outside src/infra/tracing/, and in
build.ninja libinfra_tracing.a appeared only in its own object/link/phony rules
plus the `src/all` aggregate — never in an executable or test link line (the
bnetd link line had zero tracing refs). Same class of dead lib purged in waves
75/83/91/99/116. FIX: deleted src/infra/tracing/ and the infra_tracing CMake
block including the orphaned PVPGN_V3_WITH_OTLP option; kept the live port
src/domain/shared/include/domain/shared/ports/trace_sink.hpp. Reconfigure +
relink clean (-Werror), build.ninja now has zero libinfra_tracing refs,
3199/3199 unit tests green; diff_chat / diff_channellist still match the oracle
(bnetd never linked the lib, so wire behavior is unchanged — pure build-level
dead-code removal).

## Wave 124
BNCS: emit the "No one hears you." self EID_INFO when a channel talk/emote
reaches no one but the sender. The oracle's channel_message_send
(channel.cpp:763) tracks a `heard` flag set only when a line is delivered to a
member OTHER than the sender (squelched recipients are skipped and do NOT
count); gated on conn_get_wol==0 (BNCS only), for talk/emote when `!heard` it
sends message_type_info to the sender with EMPTY username carrying "No one hears
you.". v3's BnetFsm regular-message branch and handle_emote only broadcast to
the post-squelch recipient set and emitted nothing when it was empty.
Observable: alone-talk oracle [('0x12','')] vs v3 []; alone-emote oracle
[('0x17','zoe'),('0x12','')] vs v3 [('0x17','zoe')]; two-user talk both [].
FIX (src/protocol/bnet/src/fsm/fsm_chat.cpp): in both branches, after
filter_squelched, when `recipients.empty()` send a self ChatEvent{kEidInfo,...,
username="","No one hears you."}. Empty username matches the probe (distinct
from v3's "Battle.net" command-output INFO). BNCS-only already (WolFsm is a
separate path = oracle's conn_get_wol gate); only on the successful-post path so
empty-body (->space->still posts->eligible) and over-long/dropped lines
(ChatMessage::create fails->early return->no INFO) stay correct. Guard:
tests/diff/diff_no_one_hears.py (alone talk/emote + two-user negative) PASSES on
both servers; 3199/3199 unit green; diff_talk / diff_emote / diff_squelch still
match the oracle.

## Wave 125
Hardening: a BNCS bnet packet whose 16-bit header length field exceeds the
oracle's MAX_PACKET_SIZE (3072, field_sizes.h) is treated as corrupt by the
oracle and the connection is destroyed. packet_get_size() (common/packet.cpp)
returns 0 for any declared size > MAX_PACKET_SIZE; in net_recv_packet() the
freshly-read header then yields total_size==0 < header_size(4) -> "corrupted
packet received (closing connection)" -> conn destroyed right after the header,
before the body is read. v3's BnetFramer only guarded the LOWER bound (size<4,
wave 118); it accepted any size up to 65535 and kept the session fully open and
serving.
Observable (post full BNCS login, one packet declaring N bytes): N<=3072 both
stay alive (SID_FRIENDSLIST 0x65 still replies); N in {3073,4000,60000} ORACLE
closes (no 0x65, EOF) while V3 stayed alive. Boundary exactly 3072.
FIX (src/app/bnetd/src/main/bnet_framer.hpp): add
`static constexpr std::uint16_t kMaxPacketSize = 3072;`. Valid-header path:
after reading pkt_size, `if (pkt_size > kMaxPacketSize) { wants_close=true;
break; }` (close BEFORE awaiting/decoding the body, matching the oracle).
Bad-marker resync path: extend the existing size<4 check to also close when
bad_size > kMaxPacketSize (packet_get_size returns 0 for these too regardless of
marker, so close rather than resync). wants_close is already wired to
session->close() in bnet_bnftp_dispatch.cpp.
Guards: tests/diff/diff_oversize_packet.py (sizes 100/3072 stay alive, 3073/
4000/60000 close on both servers) PASSES on both; new unit test
tests/unit/app/bnetd/bnet_framer_test.cpp (4 cases: upper-bound close,
boundary-3072 no-close, lower-bound close, bad-marker oversize close).
Full suite 3203/3203 green; diff_bad_marker_resync still matches the oracle.

## Wave 126
SID_ICONREQ (0x2D) icon filename was hardcoded "icons.bni" in v3 regardless of
clienttag. The oracle (_client_iconreq, handle_bnet.cpp:1284-1295) picks
prefs_get_war3_iconfile() (default "icons-WAR3.bni") for WAR3/W3XP clients and
prefs_get_iconfile() (default "icons.bni") for all others.
FIX (src/protocol/bnet/src/fsm/fsm_misc.cpp, BnetFsm::on(IconRequest)): choose
the reply filename from the stored client_tag_ — "icons-WAR3.bni" when
client_tag_ == kWarcraft3 || kWar3Xp, else "icons.bni". client_tag_ is already
populated from AUTH_INFO and the tag constants already exist; no new plumbing.
Guard: tests/diff/diff_iconreq.py asserts filename per product (SEXP/STAR ->
"icons.bni", W3XP/WAR3 -> "icons-WAR3.bni") matching on both servers; PASSES.
Full suite 3203/3203 green; diff_channellist still matches the oracle.

## Wave 127
WOL channel TOPIC was lost when the channel emptied. The oracle keeps IRC/WOL
topics in a SEPARATE, channel-NAME-keyed store (class_topiclist / topic.cpp)
independent of the Channel object's lifetime: _handle_topic_command writes via
Topic.set() and irc_send_topic reads via Topic.get(channel_get_name(channel)).
When the last member of a non-permanent channel leaves/disconnects the oracle
destroys the Channel (channel.cpp:558) but the topic SURVIVES, so a fresh
re-joiner gets the old topic back. v3 stored the topic ON the Channel domain
object (Channel::topic_) and removed the whole channel on empty
(leave_channel.cpp), discarding the topic -> re-joiner saw an empty 332.
Observable (WOL/sku=1000): A joins #tpersist, `TOPIC #tpersist :SecretTopic123`,
disconnects (channel destroyed on both); B re-joins -> ORACLE 332 carries
SecretTopic123, V3 carried empty.
FIX: new channel-NAME-keyed persistent topic store. Port
domain/chat/topic_store.hpp (ITopicStore: set/get by name); in-memory impl
infra/inmemory/in_memory_topic_store.hpp (case-folded key, thread-safe; mirrors
class_topiclist with no topicfile). SetChannelTopic gained an optional
ITopicStore and writes the topic keyed by channel.name() alongside the per-
channel set (set_channel_topic.cpp). WolFsm gained set_topic_store(); the JOIN
RPL_TOPIC 332 and the TOPIC-query path read from the name-keyed store (fallback
to Channel::topic() when unwired). Composition root (main.cpp) creates one
InMemoryTopicStore shared by the WOL listener; wired through make_wol_session.
Guard: tests/diff/diff_wol_topic_persist.py (A sets topic + disconnects, B
re-joins, both 332 == SecretTopic123) PASSES on both servers. Full suite
3203/3203 green; diff_wol_topic / diff_wol_topic_extraparam /
diff_wol_topic_notonchan / diff_wol_part still match the oracle.

## Wave 128
/who on a nonexistent channel now emits TWO EID_ERROR (0x13) lines to match the
oracle (_handle_who_command): "That channel does not exist." followed by the
hint "(If you are trying to search for a user, use the /whois command.)". v3's
BnetFsm::handle_who previously sent only the first line. Both the no-reader and
the find_by_name-miss paths now route through a shared channel_not_found lambda
that emits both lines (hint sent only for the not-found case, not banned-from).
Guard: tests/diff/diff_who_nonexistent.py asserts both servers emit exactly two
EID_ERROR and zero EID_INFO for "/who <bogus>" — PASSES on both. Full suite
3203/3203 green; diff_whoami / diff_channelcmds still match the oracle.

## Wave 129
WOL unknown-verb (421 ERR_UNKNOWNCOMMAND) was malformed and matched the oracle
in no path. v3's fallback in WolFsm::dispatch_line emitted
`:pvpgn.v3 421 <nick> :FOOBAR :Unknown command` — a DOUBLE colon (send_numeric
injects " :" and the caller also embedded a colon), echoing the command with the
wrong text, in BOTH the before- and after-login paths. The oracle's behavior is
state-dependent: before login it sends a well-formed
`:<server> 421 <nick> :Unrecognized command (before login)` (single trailing
colon, no command echo); after login it routes the unknown verb to the bnet
chat-command handler ("/<verb>") and sends NO 421 (observable: a server PAGE
message). FIX (wol_fsm.cpp): gate the fallback on WolState — before login
(Connecting/Authenticating) emit the oracle's exact well-formed 421 text with no
command echo; after login (Authenticated/InChannel/InGame) suppress the numeric
(return core::ok()). Updated the two stale unit tests (before-login asserts the
well-formed text + no FOOBAR echo; after-auth asserts no 421). Guard:
tests/diff/diff_wol_unknown.py — before login both emit a well-formed,
non-echoing 421; after login neither emits a 421 — PASSES on both servers. Full
suite 3203/3203 green; diff_wol_part / diff_wol_chat still match the oracle.
Corrects the false assumption in findings/wol-chat-lobby.md:469-472 that this
421 was a sane shared default. Full chat-command forwarding of "/<verb>" after
login (via fsm_chat.cpp dispatch) remains a deeper follow-up.

## Wave 130
Removed two dead composition-root static libraries: service_d2cs
(src/services/d2cs/, class D2csComposition) and service_d2dbs
(src/services/d2dbs/, class D2dbsComposition). Both were COMPILED via
add_subdirectory in src/services/CMakeLists.txt but linked into NO executable,
NO test, and NO other library. The real d2cs/d2dbs server binaries
(src/app/d2cs, src/app/d2dbs) link app_d2cs/protocol_d2cs/domain_d2cs and
app_d2dbs/domain_d2dbs directly — never the service_* libs. Whole-repo grep for
service_d2cs/service_d2dbs returned only their own CMakeLists definition lines;
grep for D2csComposition/D2dbsComposition returned only their own .cpp/.hpp plus
a docs comment in combined_composition.hpp and src/runtime/README.md. The
optional combined single-binary target (PVPGN_SINGLE_BINARY, default OFF)
mentions them only in an architecture-diagram comment and does not link them.
FIX: deleted src/services/d2cs/ and src/services/d2dbs/ entirely, removed the
two add_subdirectory(d2cs)/(d2dbs) lines and tidied the documenting comment in
src/services/CMakeLists.txt. Same category as wave 116 dead runtime/ scaffolding
removal. No observable behavior change on either server: the three server
binaries never linked these libs (linker confirms on relink), and no diff test
applies. Reconfigure + full relink clean (-Werror); unit suite 3203/3203 green,
including the pvpgn_v3_d2cs/d2dbs --help/--version smoke tests. If a future
single-binary mode runs d2cs/d2dbs in-process, that wiring is re-added then.

## Wave 131
Fixed the /version + /copyright slash-commands replying EID_ERROR (0x13)
"Unknown command." instead of the oracle's EID_INFO (0x12) output. Root cause:
BnetUseCaseContext.command_registry is never wired, so fsm_chat's full-dispatch
branch (fsm_chat.cpp:416) is always false and every non-intercepted slash
command falls into the dead-registry fallback (lines ~436-458) that only knows
/help and /who. The oracle routes /version and /copyright (and the /warranty
and /license aliases) through the CommandRegistry as plain-ASCII EID_INFO
output, so both fell through to "Unknown command" on v3.
FIX: intercept them in the fsm_chat slash-command block next to /who and /time
(new handle_version / handle_copyright). /version emits one EID_INFO line
"PvPGN <version>" (mirrors _handle_version_command PVPGN_SOFTWARE " "
PVPGN_VERSION; v3 reports its own core::kVersionString, wire shape identical).
/copyright emits the 15 fixed plain-ASCII lines byte-for-byte from
_handle_copyright_command (also via /warranty, /license). Stayed faithful to
the oracle's exact-token strstart() matching (no /ver alias — oracle has none).
Did NOT implement /uptime (no server-uptime source plumbed into the fsm yet,
and its seconds count is not cleanly diffable); it and the rest of the unrouted
family (/away /dnd /games /channels /news /finger /watch ...) remain for a
future wave that actually populates command_registry. New guard
tests/diff/diff_version.py asserts /version -> single EID 0x12 with text
starting "PvPGN", and /copyright -> identical EID sequence AND byte-identical
text on both servers; passes. Build clean (-Werror); unit suite 3203/3203
green; diff_command_case.py and diff_channelcmds.py still pass.

## Wave 132
Fixed a remote half-open-connection exhaustion surface on the shared
BNCS/BNFTP listener: v3 silently held OPEN any connection whose first byte
(the CLIENT_INITCONN_CLASS_* selector) was an unsupported/unknown class,
whereas the oracle's handle_init_packet (src/bnetd/handle_init.cpp) returns
-1 -> destroys the connection for ENC (0x04), LOCALMACHINE (0x98),
D2CS_BNETD from a non-realm IP (0x65), and every unknown byte (default).
Root cause: BnetBnftpDispatchFactory (src/app/bnetd/src/main/
bnet_bnftp_dispatch.cpp) special-cased only 0xFF/0x01 as BNet and routed
EVERY other first byte into BnftpFsm, which just waits for more bytes —
so junk init classes stayed alive (unauthenticated clients could open
unbounded half-open connections).
FIX: in the dispatch else-branch, route the first byte through the
already-unit-tested application/init dispatch_init_conn() (previously dead
code on the live path) and session->close() on kRejected / kD2csIpDenied /
kRateLimited / kD2csBnetd, mirroring the original's conn_destroy. v3 has no
realmlist so D2CS_BNETD (0x65) is passed d2cs_ip_allowed=false -> denied ->
closed, matching the oracle closing such links from a non-realm IP. Only
0x02 (BNFTP) is accepted into BnftpFsm; 0x03 (BOT) / 0x0d (TELNET) are
unimplemented in v3 but the oracle keeps those sockets OPEN (emits a
prompt), so they fall through and BnftpFsm leaves them waiting — preserving
the open/closed observable. 0xFF stays a BNet stream (intentional v3
accommodation for clients that already stripped the init byte). Wired
application_init into the bnetd target. New guard tests/diff/diff_init_class.py
asserts both servers CLOSE on 0x04/0x05/0x65/0x98/0xAB/0x00 and stay OPEN on
0x01/0x02; passes. Build clean (-Werror); unit suite 3203/3203 green;
diff_bnftp.py and diff_bad_marker_resync.py still pass.

## Wave 134
WOL LIST RPL_LISTSTART (321) emitted a spurious leading colon. The oracle
(handle_wol.cpp) sends `:<server> 321 <nick> Channel :Users Names` where
"Channel" is a middle parameter and "Users Names" the trailing one. v3's
WolFsm::send_numeric (wol_fsm.cpp) always injects " :" before its text arg,
so passing the whole "Channel :Users Names" string as the text yielded
`:<server> 321 <nick> :Channel :Users Names` (everything collapsed into one
trailing param). FIX: fold "Channel" into the target at wol_chat.cpp:161 ->
send_numeric(321, std::string(nick_) + " Channel", "Users Names"), byte-
matching the oracle. The existing guards only asserted the PRESENCE of code
321; tightened diff_wol_list.py to assert the full 321 param string (server
prefix + nick stripped) equals "Channel :Users Names" on both servers.
Build clean (-Werror); unit suite 3203/3203 green; diff_wol_list.py,
diff_wol_list_params.py, diff_wol_names.py all pass.

## Wave 135
EID_TALK (0x05) / EID_EMOTE (0x17) dropped the speaker's channel flags. When a
channel OPERATOR (tmpOP, MF_GAVEL=0x02) spoke, the SID_CHATEVENT other members
received hardcoded flags=0 (fsm_chat.cpp:536 TALK broadcast, :679 EMOTE
ChatEvent), whereas the oracle's message_bnet_format (message.cpp,
message_type_talk/message_type_emote) sets flags = conn_get_flags(me) | dstflags
— and the first joiner of a fresh non-permanent channel is tmpOP with
conn_get_flags == MF_GAVEL. Probing both servers (operator "opera" speaks,
member "memberb" reads): ORACLE TALK/EMOTE flags=0x02, V3 flags=0x00.
FIX: new private helper BnetFsm::speaker_channel_flags() looks up the channel via
channel_reader->find_by_id(current_channel_id_) (the handle_whoami idiom) and
returns chat::kMfGavel (0x02) when the speaker is the channel operator, else 0;
its result feeds the TALK broadcast and EMOTE ChatEvent `flags` in place of the
literal 0. Latency stays 0 (v3 has no per-conn latency model; non-deterministic,
not diffable). New guard tests/diff/diff_talk_op_flags.py asserts both servers
send flags=0x02 on the EID_TALK and EID_EMOTE an operator's channel-mate
receives (flags compared, latency ignored); passes. Build clean (-Werror);
unit suite 3203/3203 green; diff_talk.py and diff_join_userflags.py still pass.

## Wave 136
SID_MAPAUTHREQ1 (0x32) / SID_MAPAUTHREQ2 (0x3C) sent no reply in v3, whereas the
oracle ALWAYS answers with SERVER_MAPAUTHREPLY1/2 (same opcode) carrying a 4-byte
u32 response. The oracle's _client_mapauthreq1/2 (handle_bnet.cpp) unconditionally
creates a reply; not-in-a-game sets response = SERVER_MAPAUTHREPLY1_NO (0). v3's
BnetFsm::on(MapAuthReq1)/on(MapAuthReq2) (fsm_game.cpp) were pure no-ops (only a
login-state guard), stalling the W3 map-auth handshake.
FIX: after the existing require_clan_state guard, each handler now sends the reply
that already had a struct + encoder in v3 — MapAuthReply1{kMapAuthReply1ResponseNo}
and MapAuthReply2{0u}. v3 tracks no per-connection game/map state at the protocol
layer, so replying NO (response=0) exactly matches the oracle's not-in-a-game path,
the cleanly-diffable case. New guard tests/diff/diff_mapauth.py asserts both servers
reply 0x32/0x3C with a >=4-byte body and response==0; passes. Build clean (-Werror);
unit suite 3203/3203 green; diff_startgame_done.py still passes.

## Wave 137
WOL pre-login command gating did not mirror the oracle's con/log two-table model
(handle_wol.cpp + handle_irc_common.cpp). The oracle splits verbs into a
"connected" table valid in any state {NICK,USER,PASS,PING,PONG,QUIT,PRIVMSG,CVERS,
VERCHK,APGAR,SETOPT,SERIAL,LISTSEARCH,RUNGSEARCH,HIGHSCORE} and a "logged in"
table (LIST/TOPIC/JOIN/NAMES/PART/TIME/MODE/KICK/PAGE/FINDUSER/SET-GET CODEPAGE/
LOCALE/GETINSIDER/...); a log-table verb sent before login gets exactly one
"421 :Unrecognized command (before login)". v3 diverged three ways: (1) most
log-table handlers replied 451 ERR_NOTREGISTERED (a numeric the oracle defines but
NEVER sends); (2) the codepage/locale handlers (SET/GETCODEPAGE, SET/GETLOCALE,
GETINSIDER) had NO guard and EXECUTED pre-login, leaking 328/329/309/310/399; (3)
the con-table verb SETOPT was over-blocked with 451 where the oracle is silent.
FIX: WolFsm::dispatch_line now applies a single two-table gate — con-table verbs
dispatch in any state; every other recognized verb pre-login returns the 421
"before login" numeric and nothing else. Deleted all 22 scattered per-handler 451
"You have not registered" guards in wol_chat.cpp (dead under the gate; the 451 is
never emitted). SETOPT now routes through as a silent-ok con verb (on_setopt is a
no-op when account_id_==0), and the codepage/locale leak is closed. New guard
tests/diff/diff_wol_prelogin_gating.py asserts both servers 421 every log-table
verb pre-login, silently accept SETOPT, and emit no 421 post-login; passes.
Updated 6 unit tests that encoded the old 451 expectation (LIST/JOIN -> 421,
PRIVMSG con-table -> no 451/421). Build clean (-Werror); unit suite 3203/3203
green; diff_wol_codepage_locale / setopt / list / unknown / login still pass.

## Wave 138
Removed the dead, compiled-but-unlinked CMake library infra_persistence_realm
(src/infra/persistence/realm/: inmemory_character_repository.cpp,
filesystem_save_store.cpp, inmemory_save_store.cpp + public headers; classes
InmemoryCharacterRepository / FilesystemSaveStore / InmemorySaveStore). It was
add_subdirectory'd at src/CMakeLists.txt:909 and so built (libinfra_persistence_realm.a)
in every config, but NOTHING linked it: no production target, no unit/diff test,
and zero source references its headers/symbols outside its own dir (build.ninja
listed it only as a build product + phony aliases, never a link input). Same
dead-lib category as waves 75/83/91/123/130. FIX: deleted the whole
src/infra/persistence/realm/ subtree and removed the add_subdirectory line; also
updated four stale "(infra/persistence/realm/, future)" doc-comment references in
src/domain/d2cs and src/domain/d2dbs repository headers to "(future)".
Behavior-neutral by construction: the bnetd binary rebuilt BYTE-FOR-BYTE identical
(sha256 9d41840...), proving the lib contributed no linked symbols. Reconfigure +
build clean (-Werror); unit suite 3203/3203 green; diff_chat / diff_bnftp still pass.

## Wave 139
Fixed EID_WHISPERSENT (0x0a) target-name case canonicalization in
BnetFsm::handle_whisper (src/protocol/bnet/src/fsm/fsm_chat.cpp). When a user
whispered a target whose typed case differed from the registered account
(e.g. `/w BOB hi` to account "bob"), v3 echoed the raw-typed 'BOB' in the
sender's WHISPERSENT acknowledgement, while the oracle builds it from
conn_get_chatcharname(me, dst) — the TARGET connection's canonical account
name ('bob'). The target account is already resolved into `account` before the
ack is sent, so the username argument now uses
`account.value().name().display()` (same accessor used elsewhere in the file).
The recipient-side EID_WHISPER (0x04) already matched (carries the sender name).
New diff guard tests/diff/diff_whisper_case.py asserts `/w BOB hi` to account
"bob" yields WHISPERSENT username 'bob' on both servers. Build clean (-Werror);
unit suite 3203/3203; diff_whisper / diff_whisper_self / diff_whisper_case pass.

## Wave 140
Fixed WOL PRIVMSG text-argument extraction to mirror the original's IRC trailing
convention (WolFsm::on_privmsg, src/protocol/wol/src/wol_fsm/wol_chat.cpp). The
oracle derives a command's `text` ONLY from a leading-colon param or the first
" :" sequence (handle_irc_common.cpp:206-223) and _handle_privmsg_command then
requires `numparams>=1 && text`, otherwise emitting
`461 ... PRIVMSG :Not enough parameters` — never 411/412 and never relaying a
non-trailing remainder. v3 had rolled its own "rest after first space, optional
leading ':'" parser, diverging three ways: "PRIVMSG #chan" -> 411 (wrong numeric
+ false "no recipient"); "PRIVMSG #chan nocolon body" -> BROADCAST the body
(over-acceptance of hostile input the oracle would never relay); "PRIVMSG #chan
a:b" -> treated the mid-token colon as text. on_privmsg now applies the oracle's
trailing rule (leading colon => no target => 461; else require a " :" with a
non-empty target token before it) and the 411/412 branches are removed; the
proper "PRIVMSG #chan :msg" form still broadcasts unchanged. New diff guard
tests/diff/diff_wol_privmsg.py asserts all no-trailing-text forms collapse to an
identical 461 on both servers, the trailing form emits no numeric, and a
non-colon PRIVMSG is NOT relayed to a second channel member. Stale PRIVMSG
exclusion note in diff_wol_needmoreparams.py updated to point at the new test.
Build clean (-Werror); unit suite 3203/3203; diff_wol_privmsg /
diff_wol_needmoreparams / diff_wol_chat all pass.

## Wave 141
BNFTP unknown file packet type must not close the connection. On a BNFTP
connection (init 0x02), when a client sends a file packet whose type is neither
CLIENT_FILE_REQ (0x0100) nor CLIENT_FILE_REQ2 (0x0200), the oracle's
handle_file_packet conn_state_connected default case (src/bnetd/handle_file.cpp)
logs an error but RETURNS 0, leaving the connection in conn_state_connected so
the next packet is parsed normally and a subsequent valid CLIENT_FILE_REQ is
served. v3's BnftpFsm::try_dispatch instead set state_=Done and called
ctx_->close() on the `pkt_type != kClientFileReq` branch, tearing down the
connection BEFORE serving anything and discarding a legitimate request that
followed. Probe (init 0x02, then type=0x9999 junk, then valid REQ for a 50-byte
probe.bin): oracle returned 84 bytes (34-byte SERVER_FILE_REPLY, filelen=50, +50
body) with the socket open; v3 returned 0 bytes with the socket closed. Fix:
mirror the oracle default case — consume just the unrecognized packet
(buf_.erase) and re-invoke try_dispatch() to parse any remaining buffered/
subsequent packets, instead of closing. (This is distinct from the accepted
"v3 closes after serving one file vs oracle lingers" diff, which is unchanged.)
New diff guard tests/diff/diff_bnftp_unktype.py asserts both servers serve the
file that follows an unknown packet type (structure-normalized, timestamp
ignored). Build clean (-Werror); unit suite 3203/3203; diff_bnftp_unktype /
diff_bnftp_req2 / diff_bnftp all pass.

## Wave 142
Fixed: WOL game channels leaked into the plain LIST (327) RPL_CHANNEL section.
The original (handle_wol.cpp:576-578) skips channels whose
channel_wol_get_game_type() != 0 in the channel-list section — game lobbies are
surfaced only through the dedicated games-list path. v3's WolFsm::on_list emitted
every channel returned by list_channels_->execute(), so a JOINGAME-created lobby
(stored both as an ordinary channel in the channel repo AND as a wol_game_store_
entry) showed up mixed in with chat channels. Probe: host JOINGAME #wlgame,
control JOIN #wlchat, observer LIST -> oracle 327 tokens exclude wlgame but
include wlchat; v3 wrongly included wlgame. Fix: in on_list's emit loop, skip any
channel for which wol_game_store_->find(info.name) returns a game, mirroring the
oracle's game-type skip (the store key is the channel name without '#', which
equals info.name from the channel repo). Normal-chat and games-only/empty paths
unaffected. New diff guard tests/diff/diff_wol_list_gamechan.py asserts the
JOINGAME channel is absent from the 327 section while the JOIN channel is present,
on both servers. Build clean (-Werror); unit suite 3203/3203; diff_wol_chat /
diff_wol_gameopt also pass.

## Wave 143
Fixed: the EID_CHANNEL (SID_CHATEVENT 0x07) announcing the channel a client just
joined carried an empty USERNAME field. The original populates it with the
joiner's own chat name (message_bnet_format, message_type_channel case:
tname = conn_get_chatname(me); message.cpp:1187-1190, text = channel name). v3
hardcoded username="" for that event. Probe (alice full_login, ENTERCHAT,
JOINCHANNEL "ZZPROBECHAN"): oracle first CHATEVENT EID=0x07 user='alice'
text='ZZPROBECHAN'; v3 user='' (every other field already matched). Fix: in
BnetFsm::on(const JoinChannel&) (src/protocol/bnet/src/fsm/fsm_chat.cpp) set the
EID_CHANNEL ChatEvent username from "" to current_username_ (the joiner's chat
name, same value already used for the self-roster SHOWUSER/USERFLAGS), at both
the join-success path (:204) and the no-use-case fallback (:126). No backend or
config needed. Extended diff guard tests/diff/diff_join_channel_first_event.py to
also assert the first event's username == the logged-in name on both servers.
Build clean (-Werror); unit suite 3203/3203; diff_join_channel_first_event /
diff_chatevent_fields / diff_channel_join_edges all pass. (The unrelated
EID_INFO channel-MOTD line and self-roster MF_PLUG/statstring deltas seen in the
same join stream are backend-dependent and left aside.)

## Wave 144
SID_CHANGECLIENT (0x5C) invalid client-switch must destroy the connection (v3
no-oped). The oracle's _client_changeclient (handle_bnet.cpp, registered pre-login
in bnet_htable_con) permits a switch ONLY when the conn's current tag == W3XP AND
the requested new tag == WARCRAFT3 ("WAR3"); on any other combination it logs
"invalid attempt to change client", conn_set_state(conn_state_destroy) and FINs
the socket. On the valid path it just swaps the clienttag (no reply). v3's
BnetFsm::on(const ChangeClient&) (src/protocol/bnet/src/fsm/fsm_misc.cpp:121) was
an unconditional no-op `return core::ok()` that accepted every CHANGECLIENT and
kept the connection alive. Fix: named the param, mirrored the oracle's validity
test — client_tag_ == kWar3Xp AND requested == kWarcraft3. decode_change_client
reads the tag little-endian (RD_U32), so wire "WAR3" arrives byte-reversed vs
ClientTag's big-endian packed form; byte-swap (__builtin_bswap32) before comparing
to kWarcraft3.packed_be(). Invalid combo -> reject() (Closing + ctx_->close()),
valid combo -> set client_tag_ = kWarcraft3 and core::ok() with no reply. Also
fixed the stale header doc comment (0x68 -> 0x5C). Probe (SEXP auth_handshake +
CHANGECLIENT b"WAR3"): oracle closed / v3 was alive -> now both closed; control
(no CHANGECLIENT) alive on both. Guard tests/diff/diff_changeclient.py asserts the
invalid-path destroy + the control survival; the valid W3XP->WAR3 path is not
reachable in this harness (oracle's post-auth clienttag isn't actually W3XP) so it
is not asserted. Build clean (-Werror); unit suite green (anongame/multilocale
flakes pass on -j1); diff_advertise_part / diff_cdkey still match the oracle.

## Wave 145
WOL logged-in unknown-verb feedback. For a fully-logged-in WOL client, an
unrecognized verb makes the oracle route it to the bnet chat-command handler,
which fails with message_type_error "Unknown command." and (for a WOL conn)
renders it as a PAGE line (irc.cpp): `:<server> PAGE <nick> :Unknown command.`.
v3's logged-in fall-through (wol_fsm.cpp:314) returned core::ok() silently, so it
sent nothing. Fix: emit the PAGE line `:<server_name> PAGE <nick> :Unknown
command.` (nick "*" if unset) via send_raw, mirroring the oracle. Pre-login
unknown verbs still emit the well-formed 421; SERIAL/ADVERTC/INVDEL stay silent
no-ops. Probe (full CVERS/VERCHK/APGAR/NICK/USER login then `ZZUNKNOWN ...`):
oracle sent a PAGE-to-nick (text charset-garbled), v3 sent nothing -> now both
emit a PAGE targeting the nick. Extended guard tests/diff/diff_wol_unknown.py
with after.has_page_to_nick (verb + target only; localized text not compared).
Build clean (-Werror); unit suite 3203/3203 green; diff_wol_page /
diff_wol_prelogin_gating still match the oracle.
