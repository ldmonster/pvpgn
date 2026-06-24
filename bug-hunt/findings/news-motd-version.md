# Bug-hunt: news / MOTD / server-info / version-check / autoupdate

Subsystem comparison of ORIGINAL PvPGN (`/home/cnupt/work/pvpgn-server`) vs v3
rewrite (`/home/cnupt/work/pvpgn`).

Scope covered: `news.cpp`, `versioncheck.cpp`, `autoupdate.cpp`, and the
MOTD / SID_NEWS_INFO (0x46) / server-info / version-check reply paths in
`handle_bnet.cpp`, vs the v3 BNET protocol FSM/codec and the
`INewsStore` port + in-memory store.

Legend for classification: **BUG** (implemented but wrong on the wire / in
logic), **NOT-IMPLEMENTED** (scope gap, no behaviour), **INTENTIONAL**,
**UNSURE**.

---

## Summary table

| # | Title | Severity | Class |
|---|-------|----------|-------|
| 1 | SID_NEWS_INFO / MOTD (0x46) handler is a no-op stub — no news, no welcome packet sent | High | NOT-IMPLEMENTED |
| 2 | Version-check / CheckRevision result is hard-coded "passed"; no equation/file selection, no checksum verification | High | NOT-IMPLEMENTED |
| 3 | Classic-client MOTD (bnetd "welcome" EID_INFO text on enter-chat) not delivered | Medium | NOT-IMPLEMENTED |
| 4 | Autoupdate (SID_GETFILETIME / AUTHREPLY UPDATE) never triggers an update; FileInfoReply not produced | Medium | NOT-IMPLEMENTED |
| 5 | `INewsStore` exists but is unwired (dead port) | Low (latent) | NOT-IMPLEMENTED |
| 6 | Modern AUTH_CHECK (0x51) reply uses single `result` only — old-version (0x100) / upgrade (0x102) codes unreachable | Medium | NOT-IMPLEMENTED |

No on-the-wire structural divergences were found in the parts v3 *does*
implement (see "What matches"). The findings are predominantly **missing
behaviour**, not wrong bytes.

---

## What MATCHES (verified parity, not bugs)

- **MotdReply wire layout** (`messages_misc.hpp:85`, `codec_ladder.cpp:428`)
  exactly mirrors the original `t_server_motd_w3`
  (`bnet_protocol.h:1604`): `u8 msgtype`, `u32 curr_time`,
  `u32 first_news_time`, `u32 timestamp`, `u32 timestamp2`, then text.
  Field order and types are identical. `msg_type` default `1` matches
  `SERVER_MOTD_W3_MSGTYPE 0x01`.
- **SID code 0x46.** Original `CLIENT_MOTD_W3 / SERVER_MOTD_W3 = 0x46ff`
  (the `0xff` high byte is the legacy class encoding; the SID low-byte is
  `0x46`). v3 `kSidMotd = 0x46` (`messages_common.hpp:50`) is the correct
  1-byte SID. Match.
- **Legacy AUTHREPLY1 (0x07) result codes.** v3
  `kAuthReply1MessageBadVersion=0x00`, `Update=0x01`, `Ok=0x02`
  (`messages_legacy.hpp:69-71`) match original
  `SERVER_AUTHREPLY1_MESSAGE_BADVERSION/UPDATE/OK = 0/1/2`
  (`bnet_protocol.h:871-873`). Match (constants only; logic to pick UPDATE
  is missing — see finding #4).
- **AUTHREQ1 (0x06) CheckRevision-seed reply layout** (`AuthReq1Server`,
  `messages_legacy.hpp:51`): `u64 timestamp`, cstring filename, cstring
  equation — mirrors original `_client_progident` append order
  (`handle_bnet.cpp:660-664`). Match structurally.
- **AuthInfoReply (0x50) layout** (`codec_auth.cpp:232`): logontype,
  server_token, session_num, FILETIME(lo,hi), mpq_filename,
  checksum_formula, optional 128-byte W3 signature pad — mirrors original
  `_client_auth_info` (`handle_bnet.cpp:582-616`) including the W3/W3XP
  128-byte zero pad. Match structurally.
- **FileInfoReply (0x33) layout** (`messages_game.hpp:80`): type, unknown2,
  FILETIME, filename — matches original `t_server_filetime` /
  `_client_iconreq` style. Match structurally.

---

## Finding 1 — SID_NEWS_INFO / MOTD (0x46) handler is a no-op stub

- **Severity:** High
- **Classification:** NOT-IMPLEMENTED (the request is decoded and the reply
  type exists, but nothing is ever produced)

**Original** — `src/bnetd/handle_bnet.cpp:2884` `_client_motdw3()`:
on receiving `CLIENT_MOTD_W3` the server (a) walks the news list and sends
one `SERVER_MOTD_W3` packet per news item newer than the client's
`last_news_time` (`news_traverse(_news_cb, …)`, `_news_cb` at line 2851),
then (b) sends a final "welcome" packet whose body is `bnmotd_w3.txt`, with
`timestamp = first_news_time + 1` and `timestamp2 = SERVER_MOTD_W3_WELCOME
(0)` (lines 2908-2947). The `timestamp2==0` marker tells the client this is
the right-panel welcome/MOTD entry (the "MOTD-as-final-news-entry"
convention).

```c
news_traverse(_news_cb, &motdd);                 // one packet per news item
...
bn_int_set(&rpacket->u.server_motd_w3.timestamp,  motdd.fnews + 1);
bn_int_set(&rpacket->u.server_motd_w3.timestamp2, SERVER_MOTD_W3_WELCOME); // 0
packet_append_string(rpacket, <bnmotd_w3.txt text>);
```

**v3** — `src/protocol/bnet/src/fsm/fsm_chat.cpp:452`:

```cpp
core::Status<> BnetFsm::on(const MotdRequest&) {
    return require_clan_state(state_, "bnet fsm: MOTDREQ before login");
}
```

The handler only validates login state and returns `ok()`. It never reads
the news store, never emits any `MotdReply`, and never emits the welcome
packet. The `MotdRequest.last_news_time` field is decoded
(`codec_ladder.cpp:164`) but discarded.

**Divergence:** A WAR3/W3XP client that sends SID_NEWS_INFO receives zero
reply packets. The news ticker and the server welcome/MOTD panel are blank.
No protocol error (client tolerates absence), but the feature is absent.

**Proposed fix:** In `on(const MotdRequest& m)`, fetch news via the wired
`INewsStore::get_news()`, compute `first_news_time` (oldest returned item),
and for each item with `timestamp >= m.last_news_time` send a `MotdReply`
with `timestamp = timestamp2 = item.timestamp`. Then send a final welcome
`MotdReply` with `timestamp = first_news_time + 1`, `timestamp2 = 0`, and
`text` = configured W3 MOTD file contents. Honour newest-first vs the
original's oldest-first traversal order (original sends in list order from
`news_head.next`, which is newest-first since `_news_insert_index` keeps the
list date-descending — confirm ordering against `get_news` which already
returns newest-first).

---

## Finding 2 — Version-check / CheckRevision hard-coded "passed"

- **Severity:** High
- **Classification:** NOT-IMPLEMENTED

**Original** — version-check is a real gate:
- `versioncheck.cpp:208` `select_versioncheck(arch, client, version_id,
  checkrevision_version, checksum)` looks up the configured entry and
  returns `nullptr` if the **checksum mismatches** and `allow_bad_version`
  is false (lines 217-221).
- `versioncheck.cpp:192` `select_checkrevision(arch, client, version_id)`
  returns the per-client `{mpq_filename, equation}` (default
  `{"ver-IX86-1.mpq", "A=42 B=42 C=42 4 A=A^S B=B^B C=C^C A=A^S"}`).
- `handle_bnet.cpp:1080` / `:1208`: if `select_versioncheck` returns null and
  `allow_unknown_version` is false → `send_failed_packet()` (BADVERSION /
  AUTHREPLY1 BADVERSION). Otherwise OK, optionally UPDATE.

**v3** — `src/protocol/bnet/src/fsm/fsm_auth.cpp:56` `on(const AuthInfo&)`:

```cpp
// Phase-5 use-case will plug version-check policy here. For now we
// ack with result=0 ("passed") + empty info ...
return ctx_->send(ServerMessage{AuthCheckReply{0u, ""}});
```

and `on(const AuthCheckRequest&)` (`fsm_auth.cpp:164`) merely state-gates and
returns `ok()` — it never inspects `gameversion` / `checksum` / `exe_info`
from the request.

There is **no** `select_versioncheck` / `select_checkrevision` equivalent
anywhere in `src/application`, `src/domain`, or `src/services` (grep for
`versioncheck|CheckRevision|version_check` returns nothing outside the
protocol layer). The `AuthInfoReply.mpq_filename` / `checksum_formula`
fields exist on the struct but are never populated from a config (no
`ver-IX86-1.mpq` default constant found).

**Divergence:** Every client passes version-check unconditionally. The
checksum is never validated, the CheckRevision equation/file is never
selected per client, and a wrong/old client is never rejected. (The TODO at
`fsm_auth.cpp:66` acknowledges this as a planned "Phase-5" gap.) This is the
inverse risk of the classic "wrong constant rejects clients" bug: here a
wrong/absent constant **accepts everyone**, including clients that the
operator intends to gate.

**Proposed fix:** Introduce a version-check use-case/port carrying the
`versioncheck.json` config (per arch/client/version → equation, mpq file,
expected checksum). In `on(AuthInfo)` populate `AuthInfoReply.mpq_filename`
+ `checksum_formula` from `select_checkrevision`; in `on(AuthCheckRequest)`
verify `gameversion`/`checksum` and emit `AuthCheckReply{result, info}` with
the proper non-zero result code on failure (see finding #6 for the code
values).

---

## Finding 3 — Classic-client MOTD (welcome EID_INFO text) not delivered

- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED

**Original** — for non-W3 (classic STAR/SEXP/D2DV/…) clients the MOTD is the
text file `prefs_get_motdfile()` ("bnetd.txt"), sent as `EID_INFO`
chat-event lines on first enter-chat via `conn_send_welcome()`
(`src/bnetd/connection.cpp:112-149`, called from `connection.cpp:2023` in
the join-channel path). Each file line is delivered with
`message_send_file` → `message_send_text(type=message_type_info)` which
formats as `EID_INFO` (`message.cpp:933`).

**v3** — `src/protocol/bnet/src/fsm/fsm_chat.cpp:58` `on(EnterChatRequest)`
sends only an `EnterChatReply` (lines 64-67); `on(JoinChannel)` (line 70)
sends `EID_SHOWUSER` + `EID_CHANNEL` + broadcasts `EID_JOIN`, but emits **no**
`EID_INFO` welcome/MOTD lines. There is no `conn_send_welcome` equivalent
and no `motdfile`/`bnetd.txt` read path. (EID_INFO is only used for
join-error messages, `fsm_chat.cpp:116`.)

**Divergence:** Classic clients see no server MOTD/welcome text on login.

**Proposed fix:** On the first transition into `InChat` (enter-chat / first
join), read the configured classic MOTD file and emit one `ChatEvent`
`EID_INFO` (0x12) per line, matching `conn_send_welcome`. Gate to non-W3
clients (W3 uses the 0x46 path of finding #1); skip for IRC/WOL as the
original does (`connection.cpp:126`).

---

## Finding 4 — Autoupdate path never triggers; FileInfoReply not produced

- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED

**Original** — `autoupdate.cpp:184` `autoupdate_check(arch, client, gamelang,
versiontag, sku)` returns the per-client MPQ update file (with gamelang/SKU
suffixing for WAR3/WOL). On a matched version it is invoked from
`handle_bnet.cpp:1110` / `:1233`; if a file is returned the auth reply is
flipped to `…_MESSAGE_UPDATE` and the filename is appended. The actual file
bytes are then served via the SID_GETFILETIME / file-transfer flow.

**v3** —
- `on(FileInfoRequest)` (`fsm_auth.cpp:182`) accepts the request and returns
  `ok()` **without sending a `FileInfoReply`**. The `FileInfoReply` struct
  exists (`messages_game.hpp:80`) and encodes correctly, but nothing emits
  it. A client probing file timestamps gets no answer.
- No `autoupdate_check` equivalent exists; the `AuthInfoReply`/`AuthCheckReply`
  update branches (UPDATE result codes) are never taken (ties into findings
  #2 and #6). No `autoupdate.conf` parser found.

**Divergence:** Auto-update is entirely absent: no update file is ever
offered, and SID_GETFILETIME yields no reply (so even icon/TOS file
timestamp probes go unanswered).

**Proposed fix:** Emit a `FileInfoReply` from `on(FileInfoRequest)` (mirror
original `_client_iconreq`/filetime handlers: stat the requested file, fill
FILETIME + echo type/filename). Separately add an autoupdate config + check
to flip the auth reply to the UPDATE code when an update file matches.

---

## Finding 5 — `INewsStore` port exists but is unwired (dead code)

- **Severity:** Low (latent / housekeeping; becomes the substrate for fix #1)
- **Classification:** NOT-IMPLEMENTED

`src/domain/social/include/domain/social/ports/news_store.hpp` defines
`INewsStore { get_news(max_items) -> newest-first; add_news(item) }`, and
`src/infra/inmemory/.../in_memory_news_store.hpp` implements it. But the only
references to `INewsStore` / `get_news` / `NewsItem` in the tree are the port
+ the in-memory fake themselves — no use-case, no FSM handler, no
composition root consumes it (grep `INewsStore|get_news|NewsItem` →
only news_store.hpp + in_memory_news_store.hpp + the comment in
codec_ladder/messages). There is also no news-file (`news.txt`/`bnews.txt`)
loader equivalent to original `news_load()` (`news.cpp:129`), so even a wired
store would start empty.

**Note (potential semantic mismatch for when it IS wired):** the original
news list keeps a *default* "No news today" entry when the file is empty/
missing (`news.cpp:119` `_news_insert_default`). The in-memory store has no
such default. Not a bug today (port unused), but worth replicating so an
empty server still shows a news entry.

**Proposed fix:** When implementing finding #1, wire `INewsStore` into the
MotdRequest handler and add a news-file loader (parsing the original
`{MM/DD/YYYY}` date-block format from `news.cpp:_news_parsetime`) to populate
it, including the "No news today" default.

---

## Finding 6 — Modern AUTH_CHECK (0x51) result codes: only "passed" reachable

- **Severity:** Medium
- **Classification:** NOT-IMPLEMENTED (consequence of #2)

**Original** — legacy 0x51 reply (`SERVER_AUTHREPLY_109`) result codes
(`bnet_protocol.h:889-891`): `OK=0x00`, `UPDATE=0x100`, `BADVERSION=0x101`.
Real Battle.net SID_AUTH_CHECK additionally uses `0x100`=old game version,
`0x101`=invalid version, `0x102`=must upgrade, `0x0200`=invalid CD-key, etc.
The original selects BADVERSION vs UPDATE vs OK based on
`select_versioncheck`/`autoupdate_check` (`handle_bnet.cpp:1117-1250`).

**v3** — `AuthCheckReply{ result, info }` (`messages_auth.hpp:54`) encodes
`u32 result` + cstring (`codec_auth.cpp:225`) — the modern 0x51 wire shape,
which is structurally correct. But:
- The only value ever sent is `0u` (passed), from `on(AuthInfo)`
  (`fsm_auth.cpp:69`).
- No constants are defined for the non-zero codes (no
  `kAuthCheck*` analogues to the `kAuthReply1Message*` set), so old-version /
  upgrade / invalid-key results are unreachable.

**Divergence:** Even once a real version-check policy is added (#2), the
reply vocabulary is incomplete; the server cannot tell a client "old
version" / "must upgrade" / "invalid CD-key".

**Proposed fix:** Define the AUTH_CHECK result constants
(`0x000` passed, `0x100` old version, `0x101` invalid version,
`0x102` must upgrade, `0x0200` invalid cdkey, `0x0201` in-use,
`0x0202` banned-key) and have the version-check use-case (#2) choose the
correct one, putting the patch MPQ filename in `info` for the upgrade case.

---

## Bottom line

The v3 rewrite has the **wire structures right** wherever it implements them
(MotdReply, AuthInfoReply, AuthReply1, FileInfoReply, the 0x46 SID, the
legacy 0x07 result constants — all match the original byte-for-byte/field
order). The gaps are **behavioural / not-yet-implemented**, not protocol
corruption:

1. News + W3 welcome MOTD (0x46) handler is a stub (no packets emitted).
2. Version-check accepts everyone unconditionally (no CheckRevision/checksum
   logic; explicit "Phase-5" TODO).
3. Classic-client EID_INFO welcome MOTD is absent.
4. Autoupdate + SID_GETFILETIME reply absent.
5. `INewsStore` port present but unwired; no news-file loader.
6. AUTH_CHECK reply has only the "passed" code; old/upgrade/invalid codes
   undefined.

None of these are "implemented-but-wrong" wire bugs. The highest-risk item
for production is **#2** (everyone passes version-check), and the most
user-visible feature gap is **#1** (no news/MOTD on the W3 right panel).
