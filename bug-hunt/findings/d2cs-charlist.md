# d2cs character management — ORIGINAL vs v3 bug hunt

Subsystem: d2cs character screen flow — char list / create / delete / select +
the d2cs client protocol (CHARLISTREQ/REPLY, CREATECHARREQ/REPLY,
DELETECHARREQ/REPLY, CHARLOGINREQ/REPLY).

ORIGINAL: `/home/cnupt/work/pvpgn-server/src/d2cs/`
V3:       `/home/cnupt/work/pvpgn/src/{protocol,app,domain,application}/...`

## Architecture note — TWO parallel v3 implementations, only one is wired

The original `handle_d2cs.cpp`, `d2charfile.cpp`, `d2charlist.cpp` are **NOT
ported** into v3. v3 has two independent reimplementations of char management:

1. **The "live"/runtime path** (what actually runs):
   `protocol/d2cs` FSM (`d2cs_handlers.cpp`, `d2cs_builders.cpp`) →
   `app/d2cs/d2cs_session_handler.cpp` → `domain/d2cs` use_cases
   (`CharacterCreateUseCase`, `CharacterListUseCase`, `CharacterDeleteUseCase`,
   `CharacterSelectUseCase`) → `domain/d2cs/in_memory_repositories.hpp`.
   The egress is `app/d2cs/d2cs_tcp_session.cpp`, which calls the
   `D2CSSessionFsm::make_*_reply()` builders.

2. **A second, mostly-unwired path**: `application/realm` use_cases
   (`create_character.cpp`, `list_characters.cpp`, `delete_character.cpp`,
   `load_character.cpp`) over `domain/realm` (`Character`, `CharacterList`), and
   a separate **byte-accurate** `protocol/d2cs/charlistreply_encoder.hpp`
   reachable only through the C-ABI bridge
   `app/d2cs/src/send_charlistreply_bridge.cpp` (`pvpgn_v3_d2cs_send_charlistreply`).
   Nothing in the session handler / egress calls this bridge.

The bugs below are split by which path they affect. The most severe ones are in
the **live FSM path**, because that is what a real client talks to. The
`charlistreply_encoder.hpp` is correct but dead; `make_char_list_reply()` is the
one that ships and it is broken.

Confirmation that payload offsets start *after* the 3-byte header:
`src/protocol/d2cs/src/fsm.cpp:60-65` — `payload = buffer_.data()+kHeaderSize`,
`payload_len = packet_len - 3`. So every "Wire layout (after 3-byte header)"
comment in `d2cs_handlers.cpp` is measured from the right place; the field
contents are what's wrong.

---

## FINDING 1 — CHARLISTREPLY wire format is structurally wrong in the live path

**Severity: CRITICAL — BUG**

The live egress (`d2cs_tcp_session.cpp:101-110 send_char_list`) builds the reply
with `D2CSSessionFsm::make_char_list_reply()`, which emits:

`src/protocol/d2cs/src/fsm/d2cs_builders.cpp:75-94`
```cpp
// Header(3) + char_count(4) + names (each null-terminated)
push_header(v, total, D2CSPacketType::CHARLISTREPLY);   // type 0x17 OK
push_u32le(v, static_cast<uint32_t>(char_names.size())); // <-- bogus 4-byte count
for (const auto& n : char_names) { ...name...; push_back(0x00); }  // no portrait
```

ORIGINAL CHARLISTREPLY body (`src/common/d2cs_protocol.h:326-337`,
emitted in `src/d2cs/handle_d2cs.cpp:852-930`):
```
bn_short maxchar; bn_short currchar; bn_short u1(=0); bn_short currchar2;
then per char: charname '\0'  +  portrait block '\0'
```

Divergences (all break the real client):
- v3 writes a single 4-byte `char_count` where the protocol has four 2-byte
  fields `maxchar/currchar/u1/currchar2`. Byte count, field count and alignment
  are all wrong.
- v3 emits **no portrait block** after each name. The original always appends
  the portrait (`packet_append_string(rpacket,(char*)&ccharlist->charinfo->portrait)`,
  `handle_d2cs.cpp:889,903`). The portrait carries class/level/status/expansion/
  ladder bytes the char screen renders — so the screen would show names with no
  portraits/levels even in the best case, and more likely desync/garbage.
- `maxchar` (the "create new char allowed / slots free" signal) is never sent,
  so the client can't tell whether new-char creation is permitted.

This is the central charlist bug: the reply the shipped server sends does not
match the D2 protocol. The correct encoder exists
(`charlistreply_encoder.hpp`, which *does* emit maxchar/currchar/u1/currchar2 +
name + portrait, see lines 62-88) but is never invoked by the session path.

**Proposed fix:** route `send_char_list` through the
`charlistreply_encoder.hpp` encoder (or rewrite `make_char_list_reply` to emit
maxchar/currchar/u1/currchar2 + name + portrait), and plumb the portrait bytes
and the `maxchar`/allow-newchar value through `domain::d2cs::CharacterInfo`
(which currently carries no portrait at all — see Finding 7).

---

## FINDING 2 — CREATECHARREQ parsed with the wrong fixed header (seqno+byte class)

**Severity: CRITICAL — BUG**

`src/protocol/d2cs/src/fsm/d2cs_handlers.cpp:275-311` parses, after the 3-byte
header:
```
seqno   (uint32, 4 bytes)
char_class (uint8, 1 byte)
char_flags (uint8, 1 byte)
char_name (cstring)
```

ORIGINAL CREATECHARREQ (`src/common/d2cs_protocol.h:65-73`):
```
bn_short chclass;   // 2 bytes
bn_short u1;        // 2 bytes (always zero)
bn_short status;    // 2 bytes
/* character name */
```
i.e. 6 fixed bytes, **no seqno**, class is a 16-bit field, status is 16-bit.

Divergence: v3 reads a non-existent 4-byte seqno first, then takes class/flags
as single bytes. Net effect: `char_class` is read from offset 4 (the high half
of the real `chclass`/`u1` region, normally 0), `char_flags` from offset 5,
and `char_name` from offset 6 instead of offset 6 by coincidence of length —
but the *class* and *status/flags* values are read from the wrong bytes. With
real little-endian data, `chclass` low byte sits at offset 0, so v3's
`char_class = payload[4]` reads `u1`/`status` low byte, not the class. Character
class will be wrong (typically 0 = Amazon) and the hardcore/expansion/ladder
status bits are effectively lost.

**Proposed fix:** parse `chclass` (u16 LE @0), `u1` (u16 @2), `status` (u16 @4),
then the name. Drop the phantom seqno.

---

## FINDING 3 — CHARLOGINREQ parsed with 16 bytes of phantom fixed fields

**Severity: CRITICAL — BUG**

`src/protocol/d2cs/src/fsm/d2cs_handlers.cpp:74-120` parses, after the header:
```
seqno(4) char_class(4) char_level(4) char_status(4) account_name(cstr) char_name(cstr)
```
(16 fixed bytes, then two strings).

ORIGINAL CHARLOGINREQ (`src/common/d2cs_protocol.h:198-202`):
```
t_d2cs_client_header h;
/* character name */   <-- charname immediately follows the header, nothing else
```
Original handler reads only the charname:
`src/d2cs/handle_d2cs.cpp:582` `packet_get_str_const(packet,sizeof(t_client_d2cs_charloginreq),...)`
where `sizeof(...)` is just the 3-byte header. There is no account on the wire
(the account is taken from the connection via `d2cs_conn_get_account`), and no
class/level/status (those come from the loaded charinfo file).

Divergence: v3 consumes 16 bytes that don't exist, so the first 16 bytes of the
charname are eaten as integers and `read_cstring` then reads garbage / fails
("payload too short" or wrong name). Char login from the realm select screen is
broken. Even if the bytes happened to be present, v3 trusts the client-supplied
class/level/status instead of the server-side charinfo file — a correctness and
trust regression.

**Proposed fix:** CHARLOGINREQ payload is just `char_name` (cstring); account is
the session account (`account_name_` already stored at login). Remove the
seqno/class/level/status reads.

---

## FINDING 4 — DELETECHARREQ off-by-2 (reads seqno(4) instead of u1(2))

**Severity: HIGH — BUG**

`src/protocol/d2cs/src/fsm/d2cs_handlers.cpp:313-344` parses `seqno(4)` then
`char_name`.

ORIGINAL DELETECHARREQ (`src/common/d2cs_protocol.h:216-221`):
```
bn_short u1;   // 2 bytes, always zero
/* character name */
```
Original reads name at `sizeof(t_client_d2cs_deletecharreq)` = header(3)+u1(2)=5.

Divergence: v3 skips 4 bytes before the name instead of 2, so the first 2 bytes
of the real charname are consumed as part of the phantom seqno, truncating /
corrupting the name. Delete then targets the wrong (or no) character.

**Proposed fix:** skip a 2-byte `u1` (not 4-byte seqno) before reading the name.

---

## FINDING 5 — CHARLISTREQ reads seqno(4); protocol has maxchar(2)+u1(2)

**Severity: LOW — BUG (currently benign)**

`src/protocol/d2cs/src/fsm/d2cs_handlers.cpp:346-372` reads `seqno(4)`.
ORIGINAL CHARLISTREQ (`src/common/d2cs_protocol.h:319-324`) is `bn_short maxchar;
bn_short u1;`. Both are 4 fixed bytes total, and the original handler
(`handle_d2cs.cpp:827`) ignores the request body entirely (it rebuilds maxchar
from prefs), so v3 ignoring its (misnamed) `seqno` is functionally equivalent
*today*. Flagged because the field is mislabeled and the same 4-byte assumption
is shared with CHARLISTREQ110 (also fine since body ignored). No client-visible
break, but fix the naming to avoid future drift.

**Proposed fix:** parse `maxchar(u16)`,`u1(u16)` (or just skip 4 bytes with a
comment that the body is unused).

---

## FINDING 6 — No char_class range validation (0..6) on create

**Severity: MEDIUM — BUG**

Live path: `app/d2cs/d2cs_session_handler.cpp:146-162 handle_create_char` casts
`req.char_class` straight into `domain::d2cs::CharacterClass`
(`info.class_ = static_cast<...>(req.char_class)`), and
`domain/d2cs/use_cases.hpp:116-146 CharacterCreateUseCase::execute` validates
only the **name** — never the class. The `application/realm`
`create_character.cpp:18-31` likewise validates name length only, no class.

ORIGINAL `d2char_create` (`src/d2cs/d2charfile.cpp:161`):
```cpp
if (chclass > D2CHAR_MAX_CLASS) chclass = 0;   // D2CHAR_MAX_CLASS == 0x06
```
Class is clamped to a valid 0..6 value; the create then switch-selects a newbie
template per class (`d2charfile.cpp:197-220`).

Divergence: a client (or a fuzzer) sending class 7..255 yields an out-of-range
enum value stored as the character's class. `domain::d2cs::CharacterClass` is a
`uint8_t` enum with only 0..6 defined (`types.hpp:27-46`); values >6 are an
invalid enumerator. Downstream code that switches on class, or the
ladder/portrait that maps class→template, gets undefined behavior / a corrupt
character. Note original *clamps to 0* rather than rejecting; matching that is
safest for compatibility, though rejecting with CREATECHARREPLY_FAILED is also
defensible.

**Proposed fix:** in `CharacterCreateUseCase::execute` (and the realm one),
reject or clamp `char_class > 6` before persisting.

---

## FINDING 7 — No per-account character limit (default 8) in the live path

**Severity: MEDIUM — BUG**

Live path create: `domain/d2cs/use_cases.hpp:128-142 CharacterCreateUseCase`
validates name + duplicate only, then `repo_.save_character`. The repo
(`in_memory_repositories.hpp:77-89 save_character`) appends unconditionally.
There is **no maxchar / capacity check anywhere** in the d2cs domain path.

ORIGINAL: the per-account limit is `prefs_get_maxchar()` (default 8). It gates
the char list (`handle_d2cs.cpp:851,872 if (n >= maxchar) break;`) and the
"new char allowed" signal (`maxchar` field of CHARLISTREPLY + `prefs_allow_newchar`,
`handle_d2cs.cpp:874-879`). The client uses the `maxchar` field to enable/disable
the Create button, so the original effectively enforces the cap at the UI; the
v3 live path neither caps stored characters nor sends `maxchar` at all (see
Finding 1), so creation is unbounded.

NOTE: the *other*, unwired path does enforce it —
`domain/realm/character_list.cpp:75-93 add()` checks `is_full()` and
`application/realm/create_character.cpp:41-44` returns `ResourceExhausted`. So
the limit exists in v3 but only in the path that isn't connected to the FSM.

**Proposed fix:** enforce a configurable max (default 8) in
`CharacterCreateUseCase` and surface it through CHARLISTREPLY's `maxchar` field
once Finding 1 is fixed.

---

## FINDING 8 — Character name validation diverges from D2 rules

**Severity: MEDIUM — BUG**

Live path: `domain/d2cs/use_cases.hpp:41-49 is_valid_char_name`:
```cpp
if (name.size() < 2 || name.size() > 15) return false;
for (char c : name) if (!isalnum(c) && c != '_') return false;
```
i.e. 2..15, `[A-Za-z0-9_]`, allows leading digit, disallows `-` and `.`.

ORIGINAL `d2char_check_charname` (`src/d2cs/d2charfile.cpp:651-670`):
```cpp
if (!std::isalpha(name[0])) return -1;        // must START with a letter
for (i=1; i<=MAX_CHARNAME_LEN; i++) {
    if (isalpha(ch)) continue;
    if (ch=='-') continue;
    if (ch=='_') continue;
    if (ch=='.') continue;
    return -1;                                 // anything else rejected
}
```
So original: first char must be a **letter**; subsequent chars must be
letters or `- _ .` — **digits are NOT allowed anywhere**.

Divergences (both directions):
- v3 ACCEPTS digits (`isalnum`) — original rejects all digits. v3 accepts
  `"7thHell"`; original rejects it.
- v3 REJECTS `-` and `.` — original accepts them (`"Foo-Bar"`, `"Mr.X"`).
- v3 enforces a min length of 2; original's length check is effectively a no-op
  (`if (i >= MIN_CHARNAME_LEN || i <= MAX_CHARNAME_LEN) return 0;` — an OR that
  is always true, so original does not actually enforce min/max length here;
  length is bounded only by `MAX_CHARNAME_LEN` in the read loop / save buffer).

The task description's stated rule ("letters + one `-` and one `_`, not at
start/end") matches neither implementation exactly — the original allows
multiple `-`/`_`/`.` and the v3 allows digits but no `-`. The live v3 differs
from the shipped original, which is the concrete regression.

**Proposed fix:** align `is_valid_char_name` with original semantics: first char
alpha, remaining chars alpha or `-`/`_`/`.`; reject digits. (If the project
intends the stricter "one `-`/one `_`, not at edges" D2-client rule, apply that
consistently and document it.)

---

## FINDING 9 — DELETECHARREQ: no in-game / multilogin guard before delete

**Severity: MEDIUM — BUG**

Live path `handle_delete_char` (`d2cs_session_handler.cpp:164-171`) →
`CharacterDeleteUseCase::execute` → `repo_.delete_character(account, name)`. The
only ownership enforcement is "the character must be under this account" (the
repo keys by account). No check that the character is currently logged in / in a
game.

ORIGINAL `on_client_deletecharreq` (`src/d2cs/handle_d2cs.cpp:641-645`):
```cpp
if (conn_check_multilogin(c,charname)<0) {
    eventlog(...,"character {} is already logged in",charname);
    return -1;            // refuses to delete a logged-in character
}
d2cs_conn_set_charname(c,NULL);
```

Divergence: v3 lets you delete a character that is actively playing on a game
server (it also never clears any active char selection). In the original this is
blocked to avoid deleting a char that a d2gs still holds. The character-lock
machinery exists in v3 (`application/realm/character_lock.cpp`,
`domain/realm/character.hpp:44-48 is_locked/lock/unlock`) but the live d2cs
delete path doesn't consult it.

**Proposed fix:** before delete, reject if the character is currently
locked/in-game (consult the lock state), mirroring `conn_check_multilogin`.

---

## FINDING 10 — CHARLOGINREQ does no lock acquisition; no expiry / bnetd handshake

**Severity: MEDIUM — BUG / partially NOT-IMPLEMENTED**

Live `handle_char_login` (`d2cs_session_handler.cpp:132-144`) just does a
repository `find_character` and replies success (0x00) / not-found (0x46). The
egress (`d2cs_tcp_session.cpp:120-126`) only ever sends 0x00 or 0x46.

ORIGINAL `on_client_charloginreq` (`src/d2cs/handle_d2cs.cpp:573-628`):
- loads the charinfo file and checks it,
- enforces **char-expiry**: if `prefs_get_char_expire_time()` elapsed, replies
  `D2CS_CLIENT_CHARLOGINREPLY_EXPIRED (0x7b)` (`handle_d2cs.cpp:597-610`),
- requires a live bnetd connection and forwards a `D2CS_BNETD_CHARLOGINREQ` to
  bnetd (`handle_d2cs.cpp:614-626`) — the actual login isn't completed locally,
- sets the connection's charinfo summary (`conn_set_charinfo`) so later
  create-game / join-game can read difficulty/expansion/hardcore/ladder.

Divergences in the live path:
- The `0x7b` EXPIRED result code is defined (`wire_types.hpp:86`) but never
  emitted — char expiry is unimplemented.
- No bnetd round-trip; login "succeeds" purely on a local lookup.
- No char lock taken on login (the lock use-case exists but isn't called), so
  multilogin / cross-GS protection is absent at select time.
- `CHARLOGINREPLY_FAILED (0x01)` is also never used by the egress.

Some of this is plausibly "wired in a later round" (other handlers in the same
file are explicit stubs), hence partial NOT-IMPLEMENTED, but as written the live
char-select path silently diverges from original semantics.

**Proposed fix:** implement expiry → 0x7b, take the character lock on successful
select, and gate on the bnetd handshake (or document these as deferred).

---

## FINDING 11 — CREATECHARREPLY result-code semantics narrowed

**Severity: LOW/MEDIUM — BUG**

Live egress `send_char_create_result` (`d2cs_tcp_session.cpp:128-132`) maps
`bool` → `0x00` (succeed) or `0x01` (FAILED). The use case
(`use_cases.hpp:128-142`) returns `false` for *both* "name invalid" and
"duplicate exists".

ORIGINAL `on_client_createcharreq` (`src/d2cs/handle_d2cs.cpp:187-216`)
distinguishes:
- duplicate / already-exists → `D2CS_CLIENT_CREATECHARREPLY_ALREADY_EXIST (0x14)`
  (`handle_d2cs.cpp:189`),
- other failure → `D2CS_CLIENT_CREATECHARREPLY_FAILED (0x01)`,
- and the protocol also defines `NAME_REJECT (0x15)` for bad names
  (`d2cs_protocol.h:84`).

Divergence: v3 collapses "already exists" and "bad name" into the generic 0x01.
The D2 client shows different messages for 0x14 ("character already exists") vs
0x15 ("name not allowed") vs 0x01, so the player gets a wrong/generic error. The
codes are even defined in `wire_types.hpp:57-60` (kCreateCharReplyAlreadyExist
0x14, kCreateCharReplyNameReject 0x15) but unused on the live path.

**Proposed fix:** have the create use case return a richer result (enum) and map
duplicate→0x14, bad-name→0x15, else→0x01.

---

## FINDING 12 — On create SUCCESS, original does NOT send CREATECHARREPLY

**Severity: LOW — BUG / behavioral divergence**

ORIGINAL `on_client_createcharreq` on success (`handle_d2cs.cpp:193-211`):
sets the connection charinfo and sends a `D2CS_BNETD_CHARLOGINREQ` to bnetd,
then `return 0` — it sends **no** CREATECHARREPLY to the client on success
(CREATECHARREPLY is sent only on the failure paths, lines 212-218). The success
acknowledgement to the client comes later via the bnetd login flow.

Live v3 (`d2cs_session_handler.cpp:159-161` + `d2cs_tcp_session.cpp:128-132`)
always sends a CREATECHARREPLY, including `0x00` on success.

Divergence: v3 sends an extra success packet the original never sent, and skips
the bnetd charlogin handshake that the original performs on create. Depending on
the client state machine this can desync the post-create transition into the
char. Flagged as divergence; the bnetd-less design may make a direct success
reply intentional, but it should be verified against a real client.

---

## What MATCHES (verified correct)

- **Packet TYPE codes** in the live `D2CSPacketType` enum
  (`fsm.hpp:54-67`) match the original exactly: LOGINREPLY 0x01,
  CREATECHARREPLY 0x02, CHARLOGINREPLY 0x07, DELETECHARREPLY 0x0a,
  CHARLISTREPLY 0x17, CONVERTCHARREPLY 0x18, CHARLISTREPLY110 0x19.
  (The doc-comment banners in `d2cs_builders.cpp`/`d2cs_handlers.cpp` list wrong
  hex — e.g. "CHARLISTREPLY (0x18)", "CREATECHARREPLY (0x03)" — but those are
  just stale comments; the actual enum constants are right.)
- **Reply result-code constants** in `wire_types.hpp:54-92` match the original
  numerically (CreateChar 0x00/0x01/0x14/0x15; CharLogin 0x00/0x01/0x46/0x7b;
  DeleteChar 0x00/0x01; CreateGame, JoinGame sets, etc.). The bug is that the
  live path doesn't *use* the richer ones (Findings 10, 11).
- **`CharListReply` / `CharListReply110` structs** in `wire_types.hpp:403-452`
  correctly model `maxchar/currchar/u1/currchar2` — they just aren't used by the
  live `make_char_list_reply` (Finding 1).
- **The standalone `charlistreply_encoder.hpp`** (`encode()`, lines 62-88) is
  byte-accurate to the original CHARLISTREPLY body (4×u16 + name + portrait +
  NULs). Correct — but dead in the session path.
- **`ClientCreateCharReq` struct** (`wire_types.hpp:143-152`) correctly models
  `chclass(u16)+u1(u16)+status(u16)` — contradicting the live handler's
  byte-level parse (Finding 2), which doesn't use this struct.
- **D2 class enum ordinals** (`domain/d2cs/types.hpp:27-46`,
  Amazon=0…Assassin=6) match the original `t_character_class`
  (`d2charfile.cpp:42-51`) and `D2CHAR_MAX_CLASS==0x06`.
- **Char list sort** ASC/DESC ordering concept is preserved
  (`charlistreply_encoder.hpp` is order-agnostic; `domain/realm/character_list.cpp`
  `list()` implements by_name/by_level/by_creation_time/by_last_played sorts,
  matching original's `prefs_get_charlist_sort` name/ctime/mtime/level options).
  Note this lives in the unwired realm path; the live d2cs path
  (`CharacterListUseCase`) returns repo insertion order with no sort applied —
  a lesser divergence subsumed by Finding 1.
- **Duplicate-name rejection on create** is present in both v3 paths
  (`use_cases.hpp:135-139`, `character_list.cpp:83-89`) and matches the
  original's already-exists handling in spirit (modulo the result code,
  Finding 11).

---

## Severity summary

| # | Sev | Class | Area |
|---|-----|-------|------|
| 1 | CRITICAL | BUG | CHARLISTREPLY body wrong (no maxchar/currchar/portrait) on live path |
| 2 | CRITICAL | BUG | CREATECHARREQ parse: phantom seqno + byte class vs short chclass/status |
| 3 | CRITICAL | BUG | CHARLOGINREQ parse: 16 phantom fixed bytes; protocol has only charname |
| 4 | HIGH | BUG | DELETECHARREQ parse off-by-2 (seqno 4B vs u1 2B) |
| 5 | LOW | BUG | CHARLISTREQ field mislabeled seqno vs maxchar/u1 (benign today) |
| 6 | MEDIUM | BUG | No char_class 0..6 validation/clamp on create |
| 7 | MEDIUM | BUG | No per-account maxchar (default 8) on live create path |
| 8 | MEDIUM | BUG | Char-name validation diverges (digits vs leading-alpha; `-`/`.`) |
| 9 | MEDIUM | BUG | Delete has no in-game/multilogin guard |
| 10 | MEDIUM | BUG/NI | Char-login: no expiry(0x7b), no lock, no bnetd handshake |
| 11 | LOW/MED | BUG | CREATECHARREPLY collapses 0x14/0x15 into 0x01 |
| 12 | LOW | BUG | Create-success sends CREATECHARREPLY original wouldn't / skips bnetd |
